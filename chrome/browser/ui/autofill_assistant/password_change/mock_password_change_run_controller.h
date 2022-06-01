// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class PasswordChangeRunDisplay;

// Mocked PasswordChangeRunController used in unit tests.
class MockPasswordChangeRunController : public PasswordChangeRunController {
 public:
  MockPasswordChangeRunController();
  ~MockPasswordChangeRunController() override;

  MOCK_METHOD(void,
              Show,
              (base::WeakPtr<PasswordChangeRunDisplay>),
              (override));
  MOCK_METHOD(void,
              SetTopIcon,
              (autofill_assistant::password_change::TopIcon),
              (override));
  MOCK_METHOD(void, SetTitle, (const std::u16string&), (override));
  MOCK_METHOD(void, SetDescription, (const std::u16string&), (override));
  MOCK_METHOD(void,
              SetProgressBarStep,
              (autofill_assistant::password_change::ProgressStep),
              (override));
  MOCK_METHOD(void,
              ShowBasePrompt,
              (const autofill_assistant::password_change::BasePrompt&),
              (override));
  MOCK_METHOD(void, OnBasePromptOptionSelected, (int), (override));
  MOCK_METHOD(void,
              ShowSuggestedPasswordPrompt,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void, OnSuggestedPasswordSelected, (bool), (override));
  base::WeakPtr<PasswordChangeRunController> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordChangeRunController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_CONTROLLER_H_
