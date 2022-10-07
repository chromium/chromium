// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_DISPLAY_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_DISPLAY_H_

#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mocked PasswordChangeRunDisplay used in unit tests.
class MockPasswordChangeRunDisplay : public PasswordChangeRunDisplay {
 public:
  MockPasswordChangeRunDisplay();
  ~MockPasswordChangeRunDisplay() override;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void,
              SetTopIcon,
              (autofill_assistant::password_change::TopIcon),
              (override));
  MOCK_METHOD(void,
              SetTitle,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void, SetDescription, (const std::u16string&), (override));
  MOCK_METHOD(void,
              SetProgressBarStep,
              (autofill_assistant::password_change::ProgressStep),
              (override));
  MOCK_METHOD(autofill_assistant::password_change::ProgressStep,
              GetProgressStep,
              (),
              (override));
  MOCK_METHOD(void,
              ShowBasePrompt,
              (const std::u16string& description,
               const std::vector<PromptChoice>&),
              (override));
  MOCK_METHOD(void,
              ShowBasePrompt,
              (const std::vector<PromptChoice>&),
              (override));
  MOCK_METHOD(void,
              ShowUseGeneratedPasswordPrompt,
              (const std::u16string&,
               const std::u16string&,
               const std::u16string&,
               const PromptChoice&,
               const PromptChoice&),
              (override));
  MOCK_METHOD(void, ClearPrompt, (), (override));
  MOCK_METHOD(void, ShowStartingScreen, (const GURL&), (override));
  MOCK_METHOD(void,
              ShowCompletionScreen,
              (autofill_assistant::password_change::FlowType flow_type,
               base::RepeatingClosure done_button_callback),
              (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, PauseProgressBarAnimation, (), (override));
  MOCK_METHOD(void, ResumeProgressBarAnimation, (), (override));
  MOCK_METHOD(void, SetFocus, (), (override));
  MOCK_METHOD(void, OnControllerGone, (), (override));

  base::WeakPtr<MockPasswordChangeRunDisplay> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordChangeRunDisplay> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_RUN_DISPLAY_H_
