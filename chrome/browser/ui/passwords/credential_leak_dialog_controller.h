// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/passwords/password_base_dialog_controller.h"
#include "ui/gfx/range/range.h"

// An interface used by the credential leak dialog for setting and retrieving
// the state.
class CredentialLeakDialogController : public PasswordBaseDialogController {
 public:
  // Called when the user cancels the dialog by clicking a button.
  virtual void OnCancelDialog() = 0;

  // Called when the user accepts the dialog by clicking a button.
  virtual void OnAcceptDialog() = 0;

  // Called when the user closes the dialog without clicking a button,
  // e.g. by pressing the Esc key.
  virtual void OnCloseDialog() = 0;

  // Returns the label for the accept button.
  virtual base::string16 GetAcceptButtonLabel() const = 0;

  // Returns the label for the cancel button.
  virtual base::string16 GetCancelButtonLabel() const = 0;

  // Returns the dialog message based on credential leak type.
  virtual base::string16 GetDescription() const = 0;

  // Returns the dialog title based on credential leak type.
  virtual base::string16 GetTitle() const = 0;

  // Checks whether the dialog should prompt user to password checkup.
  virtual bool ShouldCheckPasswords() const = 0;

  // Checks whether the dialog should show cancel button.
  virtual bool ShouldShowCancelButton() const = 0;

 protected:
  ~CredentialLeakDialogController() override = default;
};

#endif  //  CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_H_
