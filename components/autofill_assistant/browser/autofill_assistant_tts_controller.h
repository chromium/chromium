// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_

#include <string>

#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"

namespace autofill_assistant {

// Used to expose Text-to-Speech functionality in Autofill Assistant.
class AutofillAssistantTtsController {
 public:
  AutofillAssistantTtsController(content::TtsController* tts_controller);

  virtual ~AutofillAssistantTtsController();

  // Speaks the message in the given locale. Stops any ongoing TTS.
  //
  // Locale string must be in BCP 47 format, eg: "en-US", "hi-IN".
  virtual void Speak(const std::string& message, const std::string& locale);

  // Stops any ongoing TTS.
  virtual void Stop();

 private:
  content::TtsController* tts_controller_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_
