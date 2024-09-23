// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_SPEECH_RECOGNIZER_IMPL_H_
#define CHROME_SERVICES_SPEECH_SODA_SPEECH_RECOGNIZER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/speech/endpointer/endpointer.h"
#include "components/speech/speech_recognizer_fsm.h"
#include "media/mojo/common/audio_data_s16_converter.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace speech {

// The implementation of the speech recognizer that runs in the speech
// recognition service process. This class uses the Speech On-Device API (SODA)
// to provide speech recognition for the Web Speech API.
class SodaSpeechRecognizerImpl
    : public media::mojom::SpeechRecognitionSession,
      public media::mojom::SpeechRecognitionRecognizerClient,
      public media::AudioDataS16Converter,
      public media::mojom::SpeechRecognitionAudioForwarder,
      public SpeechRecognizerFsm {
 public:
  using SendAudioToSpeechRecognitionServiceCallback =
      base::RepeatingCallback<void(media::mojom::AudioDataS16Ptr audio_data)>;

  // Creates a SodaSpeechRecognizerImpl instance.
  // recognizer_remote` and `recognizer_client_receiver` are the interfaces
  // used to interact with the recognizer that generates the speech recognition
  // results. `session_receiver` and `session_client` are the interfaces used to
  // send and receive Web Speech events to and from the renderer. `continuous`
  // controls whether continuous results are returned for each recognition, or
  // only a single result.
  SodaSpeechRecognizerImpl(
      bool continuous,
      int sample_rate,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizer>
          recognizer_remote,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
          recognizer_client_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          session_client,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder);

  SodaSpeechRecognizerImpl(const SodaSpeechRecognizerImpl&) = delete;
  SodaSpeechRecognizerImpl& operator=(const SodaSpeechRecognizerImpl&) = delete;
  ~SodaSpeechRecognizerImpl() override;

  // media::mojom::SpeechRecognitionSession implementation.
  void Abort() override;
  void StopCapture() override;

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnSpeechRecognitionError() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;
  void OnSpeechRecognitionStopped() override;

  // media::mojom::SpeechRecognitionAudioForwarder:
  void AddAudioFromRenderer(media::mojom::AudioDataS16Ptr buffer) override;

 private:
  void StartRecognition();

  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr audio_data);

  // SpeechRecognizerFsm implementation.
  void DispatchEvent(const FSMEventArgs& event_args) override;
  void ProcessAudioPipeline(const FSMEventArgs& event_args) override;
  FSMState PrepareRecognition(const FSMEventArgs&) override;
  FSMState StartRecording(const FSMEventArgs& event_args) override;
  FSMState StartRecognitionEngine(const FSMEventArgs& event_args) override;
  FSMState WaitEnvironmentEstimationCompletion(
      const FSMEventArgs& event_args) override;
  FSMState DetectUserSpeechOrTimeout(const FSMEventArgs& event_args) override;
  FSMState StopCaptureAndWaitForResult(const FSMEventArgs& event_args) override;
  FSMState ProcessIntermediateResult(const FSMEventArgs& event_args) override;
  FSMState ProcessFinalResult(const FSMEventArgs& event_args) override;
  FSMState AbortSilently(const FSMEventArgs& event_args) override;
  FSMState AbortWithError(const FSMEventArgs& event_args) override;
  FSMState Abort(const media::mojom::SpeechRecognitionError& error) override;
  FSMState DetectEndOfSpeech(const FSMEventArgs& event_args) override;
  FSMState DoNothing(const FSMEventArgs& event_args) const override;
  FSMState NotFeasible(const FSMEventArgs& event_args) override;

  // Returns the time span of captured audio samples since the start of capture.
  base::TimeDelta GetElapsedTime() const;

  // Low-latency energy endpointer used to detect when speech starts and ends.
  Endpointer endpointer_;

  int num_samples_recorded_ = 0;
  bool sound_started_ = false;
  bool waiting_for_final_result_ = false;
  const int sample_rate_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::Remote<media::mojom::SpeechRecognitionSessionClient> session_client_;

  // Sends audio to the speech recognition thread on the main thread.
  SendAudioToSpeechRecognitionServiceCallback send_audio_callback_;

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_recognizer_client_{this};
  mojo::Receiver<media::mojom::SpeechRecognitionAudioForwarder>
      audio_forwarder_;

  base::WeakPtrFactory<SodaSpeechRecognizerImpl> weak_ptr_factory_{this};
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_SODA_SPEECH_RECOGNIZER_IMPL_H_
