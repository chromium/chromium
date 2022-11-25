// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_H_

#include <string>

#include "components/password_manager/core/browser/ui/password_check_referrer.h"

class GURL;

// An interface for leak detection dialog implemented by
// ManagePasswordsUIController. Allows to retrieve the current state of the tab
// and notify about user actions.
class PasswordsLeakDialogDelegate {
 public:
  // Called from the dialog controller when the dialog is hidden.
  virtual void OnLeakDialogHidden() = 0;

  // Open a new tab pointing to Password Checkup.
  virtual void NavigateToPasswordCheckup(
      password_manager::PasswordCheckReferrer referrer) = 0;

 protected:
  virtual ~PasswordsLeakDialogDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_LEAK_DIALOG_DELEGATE_H_
