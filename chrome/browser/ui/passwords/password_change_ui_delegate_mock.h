// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_DELEGATE_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_DELEGATE_MOCK_H_

#include <string>

#include "chrome/browser/ui/passwords/password_change_ui_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class PasswordChangeUIDelegateMock final : public PasswordChangeUIDelegate {
 public:
  PasswordChangeUIDelegateMock();
  PasswordChangeUIDelegateMock(const PasswordChangeUIDelegateMock&) = delete;
  PasswordChangeUIDelegateMock& operator=(const PasswordChangeUIDelegateMock&) =
      delete;
  ~PasswordChangeUIDelegateMock() override;

  MOCK_METHOD(State, GetCurrentState, (), (const, override));
  MOCK_METHOD(std::u16string, GetDisplayOrigin, (), (const, override));
  MOCK_METHOD(void, OnPrivacyNoticeAccepted, (), (override));
  MOCK_METHOD(void, StartPasswordChangeFlow, (), (override));
  MOCK_METHOD(void, OnPasswordChangeDeclined, (), (override));
  MOCK_METHOD(void, CancelPasswordChangeFlow, (), (override));
  MOCK_METHOD(void, RetryLoginCheck, (), (override));
  MOCK_METHOD(void, OpenPasswordChangeTab, (), (override));
  MOCK_METHOD(void, OpenPasswordDetails, (), (override));
  MOCK_METHOD(void, OnUiDismissed, (), (override));
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_DELEGATE_MOCK_H_
