// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_ONBOARDING_PROMPT_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_ONBOARDING_PROMPT_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}  // namespace content

// Mocked AssistantOnboardingPrompt used in unit tests.
class MockAssistantOnboardingPrompt : public AssistantOnboardingPrompt {
 public:
  MockAssistantOnboardingPrompt();
  ~MockAssistantOnboardingPrompt() override;

  MOCK_METHOD(void, Show, (content::WebContents*), (override));
  MOCK_METHOD(void, OnControllerGone, (), (override));

  base::WeakPtr<AssistantOnboardingPrompt> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAssistantOnboardingPrompt> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_ONBOARDING_PROMPT_H_
