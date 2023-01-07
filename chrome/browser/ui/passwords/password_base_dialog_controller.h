// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BASE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BASE_DIALOG_CONTROLLER_H_

// A UI controller responsible for the credential manager and credentials
// leaked dialogs.
class PasswordBaseDialogController {
 public:
  PasswordBaseDialogController() = default;

  PasswordBaseDialogController(const PasswordBaseDialogController&) = delete;
  PasswordBaseDialogController& operator=(const PasswordBaseDialogController&) =
      delete;

  virtual ~PasswordBaseDialogController() = default;

  // Returns true if account chooser dialog created by derived credential
  // manager controller is active.
  virtual bool IsShowingAccountChooser() const = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BASE_DIALOG_CONTROLLER_H_
