// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"

#include <optional>
#include <string>

#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
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

void SpeechRecognitionEventHandler::OnLanguageIdentificationEvent(
    const media::mojom::LanguageIdentificationEventPtr& event) {
  if (event->asr_switch_result ==
      media::mojom::AsrSwitchResult::kSwitchSucceeded) {
    source_language_ = event->language;
  }

  // This will make its way back to the live caption controller and bubble,
  // which has its own logic to complete regardless of the value of
  // `event->asr_switch_result`.
  language_identification_callback_.Run(event);
}

void SpeechRecognitionEventHandler::SetTranscriptionResultCallback(
    BabelOrcaSpeechRecognizer::TranscriptionResultCallback
        transcription_result_callback,
    BabelOrcaSpeechRecognizer::LanguageIdentificationEventCallback
        language_identification_callback) {
  language_identification_callback_ = language_identification_callback;
  transcription_result_callback_ = transcription_result_callback;
}

void SpeechRecognitionEventHandler::RemoveSpeechRecognitionObservation() {
  transcription_result_callback_.Reset();
  language_identification_callback_.Reset();
}

}  // namespace ash::babelorca
