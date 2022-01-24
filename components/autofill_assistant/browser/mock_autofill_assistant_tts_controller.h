// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_

#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockAutofillAssistantTtsController
    : public AutofillAssistantTtsController {
 public:
  MockAutofillAssistantTtsController();
  ~MockAutofillAssistantTtsController() override;

  MOCK_METHOD2(Speak,
               void(const std::string& message, const std::string& locale));
  MOCK_METHOD0(Stop, void());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_
