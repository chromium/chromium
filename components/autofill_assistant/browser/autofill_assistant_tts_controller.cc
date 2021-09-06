// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"

namespace autofill_assistant {

AutofillAssistantTtsController::AutofillAssistantTtsController(
    content::TtsController* tts_controller)
    : tts_controller_(tts_controller) {}

AutofillAssistantTtsController::~AutofillAssistantTtsController() {}

void AutofillAssistantTtsController::Speak(const std::string& message,
                                           const std::string& locale) {
  std::unique_ptr<content::TtsUtterance> tts_utterance =
      content::TtsUtterance::Create();
  tts_utterance->SetText(message);
  tts_utterance->SetLang(locale);
  tts_utterance->SetShouldClearQueue(true);

  tts_controller_->SpeakOrEnqueue(std::move(tts_utterance));
}

void AutofillAssistantTtsController::Stop() {
  tts_controller_->Stop();
}

}  // namespace autofill_assistant