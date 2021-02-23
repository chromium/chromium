// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_CROS_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
#define CHROME_SERVICES_SPEECH_CROS_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/speech/cloud_speech_recognition_client.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace soda {
class CrosSodaClient;
}  // namespace soda

namespace speech {
// Implementation of SpeechRecognitionRecognizer specifically for ChromeOS and
// SODA. The implementation forwards and works with SODA that actually runs in
// the ML Service; The Web instantiation should not be used, and defers to the
// superclass.
class CrosSpeechRecognitionRecognizerImpl
    : public SpeechRecognitionRecognizerImpl {
 public:
  CrosSpeechRecognitionRecognizerImpl(
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          remote,
      base::WeakPtr<SpeechRecognitionServiceImpl>
          speech_recognition_service_impl,
      const base::FilePath& binary_path,
      const base::FilePath& config_path);
  ~CrosSpeechRecognitionRecognizerImpl() override;

  static void Create(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          remote,
      base::WeakPtr<SpeechRecognitionServiceImpl>
          speech_recognition_service_impl,
      const base::FilePath& binary_path,
      const base::FilePath& config_path);

  OnRecognitionEventCallback recognition_event_callback() const {
    return recognition_event_callback_;
  }
  // SpeechRecognitionRecognizerImpl:
  void SendAudioToSpeechRecognitionServiceInternal(
      media::mojom::AudioDataS16Ptr buffer) override;

 private:
  std::unique_ptr<soda::CrosSodaClient> cros_soda_client_;
  // The callback that is eventually executed on a speech recognition event
  // which passes the transcribed audio back to the caller via the speech
  // recognition event client remote.
  OnRecognitionEventCallback recognition_event_callback_;

  const bool enable_soda_;
  const base::FilePath binary_path_, languagepack_path_;

  base::WeakPtrFactory<CrosSpeechRecognitionRecognizerImpl> weak_factory_{this};
};
}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_CROS_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
