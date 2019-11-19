// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_MOCK_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class PasswordsLeakDialogDelegateMock : public PasswordsLeakDialogDelegate {
 public:
  PasswordsLeakDialogDelegateMock();
  ~PasswordsLeakDialogDelegateMock() override;

  MOCK_METHOD0(OnLeakDialogHidden, void());
  MOCK_METHOD0(NavigateToPasswordCheckup, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsLeakDialogDelegateMock);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_MOCK_H_
