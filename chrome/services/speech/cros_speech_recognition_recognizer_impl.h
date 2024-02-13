// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_CROS_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
#define CHROME_SERVICES_SPEECH_CROS_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "chromeos/services/machine_learning/public/mojom/soda.mojom.h"
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
      media::mojom::SpeechRecognitionOptionsPtr options,
      const base::FilePath& binary_path,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::string& primary_language_name,
      const bool mask_offensive_words);
  ~CrosSpeechRecognitionRecognizerImpl() override;

  static void Create(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          remote,
      media::mojom::SpeechRecognitionOptionsPtr options,
      const base::FilePath& binary_path,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::string& primary_language_name,
      const bool mask_offensive_words);

  // SpeechRecognitionRecognizerImpl:
  void SendAudioToSpeechRecognitionServiceInternal(
      media::mojom::AudioDataS16Ptr buffer) override;

  void MarkDone() override;

  static chromeos::machine_learning::mojom::SodaMultilangConfigPtr
  AddLiveCaptionLanguagesToConfig(
      const std::string& primary_language_name,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::vector<std::string>& live_caption_languages);

 private:
  std::unique_ptr<soda::CrosSodaClient> cros_soda_client_;

  const base::FilePath binary_path_;
  base::WeakPtrFactory<CrosSpeechRecognitionRecognizerImpl> weak_factory_{this};
};
}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_CROS_SPEECH_RECOGNITION_RECOGNIZER_IMPL_H_
