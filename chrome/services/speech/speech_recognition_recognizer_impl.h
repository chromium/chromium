// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
#define CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/services/speech/audio_source_consumer.h"
#include "chrome/services/speech/speech_recognition_service_impl.h"
#include "components/soda/constants.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace soda {
class SodaClient;
}  // namespace soda

namespace speech {

class SpeechRecognitionRecognizerImpl
    : public media::mojom::SpeechRecognitionRecognizer,
      public AudioSourceConsumer,
      public SpeechRecognitionServiceImpl::Observer {
 public:
  using OnRecognitionEventCallback =
      base::RepeatingCallback<void(media::SpeechRecognitionResult event)>;

  using OnLanguageIdentificationEventCallback = base::RepeatingCallback<void(
      const std::string& language,
      const media::mojom::ConfidenceLevel confidence_level,
      const media::mojom::AsrSwitchResult asr_switch_result)>;

  using OnSpeechRecognitionStoppedCallback = base::RepeatingCallback<void()>;

  SpeechRecognitionRecognizerImpl(
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          remote,
      media::mojom::SpeechRecognitionOptionsPtr options,
      const base::FilePath& binary_path,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::string& primary_language_name,
      const bool mask_offensive_words,
      base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service =
          nullptr);

  SpeechRecognitionRecognizerImpl(const SpeechRecognitionRecognizerImpl&) =
      delete;
  SpeechRecognitionRecognizerImpl& operator=(
      const SpeechRecognitionRecognizerImpl&) = delete;

  ~SpeechRecognitionRecognizerImpl() override;

  static const char kCaptionBubbleVisibleHistogramName[];
  static const char kCaptionBubbleHiddenHistogramName[];

  // SpeechRecognitionServiceImpl::Observer:
  void OnLanguagePackInstalled(
      base::flat_map<std::string, base::FilePath> config_paths) override;

  static void Create(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          remote,
      media::mojom::SpeechRecognitionOptionsPtr options,
      const base::FilePath& binary_path,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::string& primary_language_name,
      const bool mask_offensive_words,
      base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service);

  static bool IsMultichannelSupported();

  OnRecognitionEventCallback recognition_event_callback() const {
    return recognition_event_callback_;
  }

  OnLanguageIdentificationEventCallback language_identification_event_callback()
      const {
    return language_identification_event_callback_;
  }

  OnSpeechRecognitionStoppedCallback speech_recognition_stopped_callback()
      const {
    return speech_recognition_stopped_callback_;
  }

  // Convert the audio buffer into the appropriate format and feed the raw audio
  // into the speech recognition instance.
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer) final;

  void OnSpeechRecognitionError();

  void MarkDone() override;

  // AudioSourceConsumer:
  void AddAudio(media::mojom::AudioDataS16Ptr buffer) override;
  void OnAudioCaptureEnd() override;
  void OnAudioCaptureError() override;

 protected:
  virtual void SendAudioToSpeechRecognitionServiceInternal(
      media::mojom::AudioDataS16Ptr buffer);

  // Return the transcribed audio from the recognition event back to the caller
  // via the recognition event client.
  void OnRecognitionEvent(media::SpeechRecognitionResult event);

  void OnLanguageIdentificationEvent(
      const std::string& language,
      const media::mojom::ConfidenceLevel confidence_level,
      const media::mojom::AsrSwitchResult asr_switch_result);

  void OnRecognitionStoppedCallback();

  base::flat_map<std::string, base::FilePath> config_paths() const {
    return config_paths_;
  }
  std::string primary_language_name() const { return primary_language_name_; }

  media::mojom::SpeechRecognitionOptionsPtr options_;

 private:
  void OnLanguageChanged(const std::string& language) final;

  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) final;

  void ResetSodaWithNewLanguage(
      std::string language_name,
      std::pair<base::FilePath, bool> config_and_exists);

  void RecordDuration();

  // Called as a response to sending a SpeechRecognitionEvent to the client
  // remote.
  void OnSpeechRecognitionRecognitionEventCallback(bool success);

  // Called when the client host is disconnected. Halts future speech
  // recognition.
  void OnClientHostDisconnected();

  // Reset and initialize the SODA client.
  void ResetSoda();

  // The remote endpoint for the mojo pipe used to return transcribed audio from
  // the speech recognition service to the browser process.
  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> client_remote_;

  std::unique_ptr<soda::SodaClient> soda_client_;

  // The callback that is eventually executed on a speech recognition event
  // which passes the transcribed audio back to the caller via the speech
  // recognition event client remote.
  OnRecognitionEventCallback recognition_event_callback_;

  OnLanguageIdentificationEventCallback language_identification_event_callback_;

  OnSpeechRecognitionStoppedCallback speech_recognition_stopped_callback_;

  base::flat_map<std::string, base::FilePath> config_paths_;
  std::string primary_language_name_;
  int sample_rate_ = 0;
  int channel_count_ = 0;
  bool mask_offensive_words_ = false;

  base::TimeDelta caption_bubble_visible_duration_;
  base::TimeDelta caption_bubble_hidden_duration_;

  // Whether the client is still requesting speech recognition.
  bool is_client_requesting_speech_recognition_ = true;

  // Time the most recent nonzero data was processed.
  // Used when options_->skip_continuously_empty_audio == true.
  base::Time last_non_empty_audio_time_ = base::Time::Now();

  // Whether the speech recognition session contains any recognized speech. Used
  // for logging purposes only.
  bool session_contains_speech_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service_;

  base::WeakPtrFactory<SpeechRecognitionRecognizerImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
