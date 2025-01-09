#include <iostream>
using namespace std;

#define FTYPE double
#include "olcNoiseMaker.h"

namespace synth
{
    // w es la constante de velocidad angular en física. este método coge una frecuencia y calcula su velocidad angular
    FTYPE W(const FTYPE dHertz)
    {
        /*
         * para generar una onda sinuidal hay que multiplicar 0, 5 (nuestro origen de la onda) por la función sin
         * de la nota tocada por la velocidad angular (2 * pi)
        */
        return dHertz * 2.0 * PI;
    }

    struct note
    {
        int id;
        FTYPE on;
        FTYPE off;
        bool active;
        int channel;

        note()
        {
            id = 0;
			on = 0.0;
			off = 0.0;
			active = false;
			channel = 0;
        }
    };

	const int OSC_SINE = 0;
	const int OSC_SQUARE = 1;
	const int OSC_TRIANGLE = 2;
	const int OSC_SAW = 3;
	const int OSC_NOISE = 4;

	// osciladores
	FTYPE oscillator(const FTYPE dTime, const FTYPE dHertz, const int nOscillatorType = OSC_SINE,
        const FTYPE dLFOHertz = 0.0, const FTYPE dLFOAmplitude = 0.0, FTYPE dCustomSawWaveResolution = 50.0)
	{
		FTYPE dModBaseFreq = W(dHertz) * dTime + dLFOAmplitude * dHertz * sin(W(dLFOHertz) * dTime);
		switch (nOscillatorType)
		{
		case OSC_SINE:
			return sin(dModBaseFreq);
		case OSC_SQUARE:
			return sin(dModBaseFreq) > 0 ? 1.0 : -1.0;
		case OSC_TRIANGLE:
			return asin(sin(dModBaseFreq)) * (2.0 / PI);
		case OSC_SAW:
		{
			FTYPE dOutput = 0.0;
			for (FTYPE n = 1.0; n < dCustomSawWaveResolution; n++)
				dOutput += (sin(dModBaseFreq)) / n;
			return dOutput * (2.0 / PI);
		}
		case OSC_NOISE:
			return 2.0 * ((FTYPE)rand() / (FTYPE)RAND_MAX) - 1.0;
		default:
			return 0.0;
		}
	}
    
    // conversión de escala a frecuencia
    const int SCALE_DEFAULT = 0;

    FTYPE scale(const int nNoteID, const int nScaleID = SCALE_DEFAULT)
    {
        switch (nScaleID)
        {
        case SCALE_DEFAULT: default:
            return 256 * pow(1.0594630943592952645618252949463, nNoteID);
        }
    }

    // envolventes
    struct envelope
    {
        virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff) = 0;
    };

    struct envelope_adsr : public envelope
    {
        FTYPE dAttackTime;
		FTYPE dDecayTime;
		FTYPE dSustainAmplitude;
		FTYPE dReleaseTime;
		FTYPE dStartAmplitude;

        envelope_adsr()
        {
            dAttackTime = 0.1;
            dDecayTime = 0.1;
            dSustainAmplitude = 1.0;
            dReleaseTime = 0.2;
            dStartAmplitude = 1.0;
        }

        virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff)
        {
			FTYPE dAmplitude = 0.0;
			FTYPE dReleaseAmplitude = 0.0;

            if (dTimeOn > dTimeOff)
            {
                FTYPE dLifeTime = dTime - dTimeOn;
				if (dLifeTime <= dAttackTime)
					dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dAmplitude = dSustainAmplitude;
			}
			else
			{
				FTYPE dLifeTime = dTime - dTimeOff;
				if (dLifeTime <= dAttackTime)
					dReleaseAmplitude = (dLifeTime / dAttackTime) * dSustainAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dReleaseAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

                if (dLifeTime > (dAttackTime + dDecayTime))
                    dReleaseAmplitude = dSustainAmplitude;

				dAmplitude = ((dTime - dTimeOff) / dReleaseTime) * (0.0 - dReleaseAmplitude) + dReleaseAmplitude;
            }

            if (dAmplitude <= 0.000)
                dAmplitude = 0.0;

            return dAmplitude;
        }
    };

    FTYPE env(const FTYPE dTime, envelope &env, const FTYPE dTimeOn, const FTYPE dTimeOff)
    {
        return env.amplitude(dTime, dTimeOn, dTimeOff);
    }

    struct instrument_base
    {
        FTYPE dVolume;
        synth::envelope_adsr env;
		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished) = 0;
    };

    struct instrument_bell : public instrument_base
    {
        instrument_bell()
        {
            env.dAttackTime = 0.01;
            env.dDecayTime = 1.0;
            env.dSustainAmplitude = 0.0;
            env.dReleaseTime = 1.0;

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (dAmplitude <= 0.0) bNoteFinished = true;

            FTYPE dSound =
                +1.00 * synth::oscillator(n.on - dTime, synth::scale(n.id + 12), synth::OSC_SINE, 5.0, 0.001)
                + 0.50 * synth::oscillator(n.on - dTime, synth::scale(n.id + 24))
                + 0.25 * synth::oscillator(n.on - dTime, synth::scale(n.id + 36));

            return dAmplitude * dSound * dVolume;
        }
    };

    struct instrument_harmonica : public instrument_base
    {
        instrument_harmonica()
        {
            env.dAttackTime = 0.05;
            env.dDecayTime = 1.0;
            env.dSustainAmplitude = 0.95;
            env.dReleaseTime = 0.1;

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (dAmplitude <= 0.0) bNoteFinished = true;

            FTYPE dSound =
                //+ 1.0  * synth::osc(n.on - dTime, synth::scale(n.id-12), synth::OSC_SAW_ANA, 5.0, 0.001, 100)
                +1.00 * synth::oscillator(n.on - dTime, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
                + 0.50 * synth::oscillator(n.on - dTime, synth::scale(n.id + 12), synth::OSC_SQUARE)
                + 0.05 * synth::oscillator(n.on - dTime, synth::scale(n.id + 24), synth::OSC_NOISE);

            return dAmplitude * dSound * dVolume;
        }
    };
}

vector<synth::note> vecNotes;
mutex muxNotes;
synth::instrument_bell instBell;
synth::instrument_harmonica instHarmonica;

typedef bool(*lambda)(synth::note const& item);
template<class T>
void safe_remove(T& v, lambda f)
{
	auto n = v.begin();
    while (n != v.end())
        if (!f(*n))
            n = v.erase(n);
        else
            ++n;
}

FTYPE GenerateTone(FTYPE dTime)
{
	unique_lock<mutex> lm(muxNotes);
	FTYPE dMixedOutput = 0.0;

    for (auto& n : vecNotes)
    {
        bool bNoteFinished = false;
        FTYPE dSound = 0.0;
        if (n.channel == 2)
			dSound = instBell.sound(dTime, n, bNoteFinished);
        if (n.channel == 1)
			dSound = instHarmonica.sound(dTime, n, bNoteFinished) * 0.5;
        dMixedOutput += dSound;

		if (bNoteFinished && n.off > n.on)
			n.active = false;
    }

    safe_remove<vector<synth::note>>(vecNotes, [](synth::note const& item) { return item.active; });

	return dMixedOutput * 0.4;
}

int main()
{
    wcout << "Pepe Hinojo Caparros - Sintetizador Basico" << endl;

    // conseguir y mostrar todos los dispositivos de audio del equipo
    vector<wstring> devices = olcNoiseMaker<short>::Enumerate();
    for (auto d : devices) wcout << "Dispositivo de salida de audio encontrado:" << d << endl;
    wcout << "Dispositivo en uso:" << devices[0] << endl;

    wcout << endl
        << "|   |   | |   |   |   |   | |   | |   |   |   |  " << endl
        << "|   | S | | D |   |   | G | | H | | J |   |   |  " << endl
        << "|   |___| |___|   |   |___| |___| |___|   |   |__" << endl
        << "|     |     |     |     |     |     |     |     |" << endl
        << "|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |" << endl
        << "|_____|_____|_____|_____|_____|_____|_____|_____|" << endl << endl;
    
    // crear generador de ruidos
    olcNoiseMaker<short> toneGenerator(devices[0], 44100, 1, 8, 512);
    toneGenerator.SetUserFunction(GenerateTone);

    char keyboard[129];
	memset(keyboard, ' ', 127);
	keyboard[128] = '\0';

	auto clock_old_time = chrono::high_resolution_clock::now();
	auto clock_real_time = chrono::high_resolution_clock::now();
    double dElapsedTime = 0.0;

    while (1)
    {
        for (int k = 0; k < 16; k++)
        {
            short nKeyState = GetAsyncKeyState((unsigned char)("ZSXDCVGBHNJM/xbc"[k]));
            double dTimeNow = toneGenerator.GetTime();

            muxNotes.lock();
            auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synth::note const& item) { return item.id == k; });
            if (noteFound == vecNotes.end())
            {
                if (nKeyState & 0x8000)
                {
                    synth::note n;
                    n.id = k;
                    n.on = dTimeNow;
                    n.channel = 1;
                    n.active = true;

					vecNotes.emplace_back(n);
                }
                else
                {

                }
            }
            else
            {
                if (nKeyState & 0x8000)
                {
                    if (noteFound->off > noteFound->on)
                    {
						noteFound->on = dTimeNow;
						noteFound->active = true;
                    }
				}
                else
                {
                    if (noteFound->off < noteFound->on)
                    {
                        noteFound->off = dTimeNow;
                    }
                }
            }
        }

        wcout << "\rNotas: " << vecNotes.size() << "         ";
    }

    return 0;
}