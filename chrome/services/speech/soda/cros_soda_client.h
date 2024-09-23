// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_CROS_SODA_CLIENT_H_
#define CHROME_SERVICES_SPEECH_SODA_CROS_SODA_CLIENT_H_

#include "base/functional/callback.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/soda.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace soda {

// Client that wraps the ML Service connection for SODA on Chrome.
// TODO(robsc@chromium): Move this to
// chromeos/services/machine_learning/public/cpp as SodaClient.
class CrosSodaClient : public chromeos::machine_learning::mojom::SodaClient {
 public:
  CrosSodaClient();
  ~CrosSodaClient() override;

  using TranscriptionResultCallback =
      base::RepeatingCallback<void(media::SpeechRecognitionResult event)>;

  using OnStopCallback = base::RepeatingCallback<void()>;
  using OnLanguageIdentificationEventCallback = base::RepeatingCallback<void(
      const std::string& language,
      const media::mojom::ConfidenceLevel confidence_level,
      const media::mojom::AsrSwitchResult asr_switch_result)>;

  // Adds audio to this soda client. Only makes sense when initialized.
  // Eventually, asynchronous callbacks to the ::SodaClient overrides below are
  // executed.
  void AddAudio(const char* audio_buffer, int audio_buffer_size) const;

  // Notifies the soda client to stop speech recognition after processing the
  // audio it has received so far.
  void MarkDone();

  // Checks if the sample rate / channels changed between calls.
  bool DidAudioPropertyChange(int sample_rate, int channel_count);
  bool IsInitialized() const { return is_initialized_; }

  // chromeos::machine_learning::mojom::SodaClient:
  void OnStop() override;
  void OnStart() override;
  void OnSpeechRecognizerEvent(
      chromeos::machine_learning::mojom::SpeechRecognizerEventPtr event)
      override;

  // Reset this client with the provided configuration, and send recognition
  // callbacks of (text, is_final) to the given callback.
  void Reset(chromeos::machine_learning::mojom::SodaConfigPtr soda_config,
             TranscriptionResultCallback transcription_callback,
             OnStopCallback stop_callback,
             OnLanguageIdentificationEventCallback langid_callback);

 private:
  // This callback is called with (media::mojom::SpeechRecognitionResult)
  // whenever soda responds appropriately.
  TranscriptionResultCallback transcription_callback_;

  // This callback is called with transcription stops.
  OnStopCallback stop_callback_;

  // This callback is called whenever langid sends a change/confidence event.
  OnLanguageIdentificationEventCallback langid_callback_;

  bool is_initialized_ = false;
  int sample_rate_ = 0;
  int channel_count_ = 0;

  mojo::Remote<chromeos::machine_learning::mojom::SodaRecognizer>
      soda_recognizer_;
  mojo::Receiver<chromeos::machine_learning::mojom::SodaClient> soda_client_;
  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;
};
}  // namespace soda
#endif  // CHROME_SERVICES_SPEECH_SODA_CROS_SODA_CLIENT_H_
