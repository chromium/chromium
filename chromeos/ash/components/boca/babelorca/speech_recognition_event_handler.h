// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SPEECH_RECOGNITION_EVENT_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SPEECH_RECOGNITION_EVENT_HANDLER_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

// This class implements the behavior of the BabelOrcaSpeechReocgnizerImpl for
// handling speech recognition events.
//
// TODO(376671280): Handle LanguageIdentificationEvents.
class SpeechRecognitionEventHandler {
 public:
  explicit SpeechRecognitionEventHandler(const std::string& source_language);
  ~SpeechRecognitionEventHandler();
  SpeechRecognitionEventHandler(const SpeechRecognitionEventHandler&) = delete;
  SpeechRecognitionEventHandler& operator=(
      const SpeechRecognitionEventHandler&) = delete;

  // Set and unset callback for transcription results
  void SetTranscriptionResultCallback(
      BabelOrcaSpeechRecognizer::TranscriptionResultCallback callback);
  void RemoveTranscriptionResultObservation();

  // Called by the speech recognizer when a transcript is received.
  void OnSpeechResult(
      const std::optional<media::SpeechRecognitionResult>& result);

 private:
  std::string source_language_;

  BabelOrcaSpeechRecognizer::TranscriptionResultCallback
      transcription_result_callback_;
};

}  // namespace ash::babelorca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SPEECH_RECOGNITION_EVENT_HANDLER_H_
