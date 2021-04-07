// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
#define CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/services/speech/cloud_speech_recognition_client.h"
#include "components/soda/constants.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace soda {
class SodaClient;
}  // namespace soda

namespace speech {
class SpeechRecognitionServiceImpl;

class SpeechRecognitionRecognizerImpl
    : public media::mojom::SpeechRecognitionRecognizer {
 public:
  using OnRecognitionEventCallback =
      base::RepeatingCallback<void(const std::string& result,
                                   const bool is_final)>;
  using OnLanguageIdentificationEventCallback = base::RepeatingCallback<void(
      const std::string& language,
      const media::mojom::ConfidenceLevel confidence_level)>;
  SpeechRecognitionRecognizerImpl(
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          remote,
      base::WeakPtr<SpeechRecognitionServiceImpl>
          speech_recognition_service_impl,
      const base::FilePath& binary_path,
      const base::FilePath& config_path);
  ~SpeechRecognitionRecognizerImpl() override;

  static const char kCaptionBubbleVisibleHistogramName[];
  static const char kCaptionBubbleHiddenHistogramName[];

  static void Create(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          remote,
      base::WeakPtr<SpeechRecognitionServiceImpl>
          speech_recognition_service_impl,
      const base::FilePath& binary_path,
      const base::FilePath& config_path);

  static bool IsMultichannelSupported();

  OnRecognitionEventCallback recognition_event_callback() const {
    return recognition_event_callback_;
  }

  OnLanguageIdentificationEventCallback language_identification_event_callback()
      const {
    return language_identification_event_callback_;
  }

  // Convert the audio buffer into the appropriate format and feed the raw audio
  // into the speech recognition instance.
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer) final;

  void OnSpeechRecognitionError();

 protected:
  virtual void SendAudioToSpeechRecognitionServiceInternal(
      media::mojom::AudioDataS16Ptr buffer);

  // Return the transcribed audio from the recognition event back to the caller
  // via the recognition event client.
  void OnRecognitionEvent(const std::string& result, const bool is_final);
  void OnLanguageIdentificationEvent(
      const std::string& language,
      const media::mojom::ConfidenceLevel confidence_level);

 private:
  void OnCaptionBubbleClosed() final;

  void AudioReceivedAfterBubbleClosed(base::TimeDelta duration) final;

  void OnLanguageChanged(const std::string& language) final;

  void RecordDuration();

  // Reset and initialize the SODA client.
  void ResetSoda();

  // The remote endpoint for the mojo pipe used to return transcribed audio from
  // the speech recognition service back to the renderer.
  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> client_remote_;

  bool enable_soda_ = false;
  std::unique_ptr<soda::SodaClient> soda_client_;

  std::unique_ptr<CloudSpeechRecognitionClient> cloud_client_;

  // The callback that is eventually executed on a speech recognition event
  // which passes the transcribed audio back to the caller via the speech
  // recognition event client remote.
  OnRecognitionEventCallback recognition_event_callback_;

  OnLanguageIdentificationEventCallback language_identification_event_callback_;

  base::FilePath config_path_;
  int sample_rate_ = 0;
  int channel_count_ = 0;
  LanguageCode language_ = LanguageCode::kNone;

  base::TimeDelta caption_bubble_visible_duration_;
  base::TimeDelta caption_bubble_hidden_duration_;
  bool caption_bubble_closed_ = false;

  base::WeakPtrFactory<SpeechRecognitionRecognizerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionRecognizerImpl);
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
