// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_CONTROLLER_H_

#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

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
  MOCK_METHOD(
      void,
      ShowBasePrompt,
      (const autofill_assistant::password_change::BasePromptSpecification&),
      (override));
  MOCK_METHOD(void, OnBasePromptChoiceSelected, (size_t), (override));
  MOCK_METHOD(void,
              ShowUseGeneratedPasswordPrompt,
              (const autofill_assistant::password_change::
                   UseGeneratedPasswordPromptSpecification&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void, ShowStartingScreen, (const GURL&), (override));
  MOCK_METHOD(void,
              ShowCompletionScreen,
              (base::RepeatingClosure done_button_callback),
              (override));
  MOCK_METHOD(void, OpenPasswordManager, (), (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, OnGeneratedPasswordSelected, (bool), (override));
  MOCK_METHOD(bool, PasswordWasSuccessfullyChanged, (), (override));
  base::WeakPtr<PasswordChangeRunController> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordChangeRunController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_CONTROLLER_H_
