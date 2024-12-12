// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"

#include <optional>
#include <string>

#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

SpeechRecognitionEventHandler::SpeechRecognitionEventHandler(
    const std::string& source_language)
    : source_language_(source_language) {}
SpeechRecognitionEventHandler::~SpeechRecognitionEventHandler() = default;

void SpeechRecognitionEventHandler::OnSpeechResult(
    const std::optional<media::SpeechRecognitionResult>& result) {
  if (transcription_result_callback_ && result.has_value()) {
    transcription_result_callback_.Run(result.value(), source_language_);
  }
}

void SpeechRecognitionEventHandler::SetTranscriptionResultCallback(
    BabelOrcaSpeechRecognizer::TranscriptionResultCallback callback) {
  transcription_result_callback_ = callback;
}

void SpeechRecognitionEventHandler::RemoveTranscriptionResultObservation() {
  transcription_result_callback_.Reset();
}

}  // namespace ash::babelorca
