// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_MOCK_H_

#include <string>

#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

class PasswordsLeakDialogDelegateMock : public PasswordsLeakDialogDelegate {
 public:
  PasswordsLeakDialogDelegateMock();

  PasswordsLeakDialogDelegateMock(const PasswordsLeakDialogDelegateMock&) =
      delete;
  PasswordsLeakDialogDelegateMock& operator=(
      const PasswordsLeakDialogDelegateMock&) = delete;

  ~PasswordsLeakDialogDelegateMock() override;

  MOCK_METHOD(void, OnLeakDialogHidden, (), (override));
  MOCK_METHOD(void,
              NavigateToPasswordCheckup,
              (password_manager::PasswordCheckReferrer),
              (override));
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_MOCK_H_
