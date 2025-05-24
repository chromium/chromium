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

void SpeechRecognitionEventHandler::AddObserver(
    BabelOrcaSpeechRecognizer::Observer* obs) {
  observers_.AddObserver(obs);
}

void SpeechRecognitionEventHandler::RemoveObserver(
    BabelOrcaSpeechRecognizer::Observer* obs) {
  observers_.RemoveObserver(obs);
}

void SpeechRecognitionEventHandler::OnSpeechResult(
    const std::optional<media::SpeechRecognitionResult>& result) {
  if (result.has_value()) {
    NotifySpeechResult(result.value());
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
  NotifyLanguageIdentificationEvent(std::move(event));
}

void SpeechRecognitionEventHandler::NotifySpeechResult(
    const media::SpeechRecognitionResult& result) {
  for (auto& observer : observers_) {
    observer.OnTranscriptionResult(result, source_language_);
  }
}

void SpeechRecognitionEventHandler::NotifyLanguageIdentificationEvent(
    const media::mojom::LanguageIdentificationEventPtr& ptr) {
  for (auto& observer : observers_) {
    // Clone here in case there are multiple observers.
    observer.OnLanguageIdentificationEvent(ptr.Clone());
  }
}

}  // namespace ash::babelorca
