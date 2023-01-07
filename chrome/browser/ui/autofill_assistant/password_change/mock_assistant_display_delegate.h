// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_DISPLAY_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_DISPLAY_DELEGATE_H_

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/view.h"

// Mocked AssistantDisplayDelegate used in unit tests.
class MockAssistantDisplayDelegate : public AssistantDisplayDelegate {
 public:
  MockAssistantDisplayDelegate();
  ~MockAssistantDisplayDelegate() override;

  MOCK_METHOD(views::View*,
              SetView,
              (std::unique_ptr<views::View>),
              (override));
  MOCK_METHOD(views::View*, GetView, (), (override));
  MOCK_METHOD(void, RemoveView, (), (override));
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_DISPLAY_DELEGATE_H_
