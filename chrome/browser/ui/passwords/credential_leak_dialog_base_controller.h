// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_BASE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_BASE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "url/gurl.h"

class CredentialLeakPrompt;
class PasswordsLeakDialogDelegate;

// A UI abstract controller responsible for the credential leak dialog. The
// class captures common functionality of the different leak dialog controllers.
class CredentialLeakDialogBaseController
    : public CredentialLeakDialogController {
 public:
  CredentialLeakDialogBaseController(
      PasswordsLeakDialogDelegate* delegate,
      password_manager::metrics_util::LeakDialogType dialog_type);

  CredentialLeakDialogBaseController(
      const CredentialLeakDialogBaseController&) = delete;
  CredentialLeakDialogBaseController& operator=(
      const CredentialLeakDialogBaseController&) = delete;

  ~CredentialLeakDialogBaseController() override;

  // Pop up the credential leak dialog.
  void ShowCredentialLeakPrompt(CredentialLeakPrompt* dialog);

  // CredentialLeakDialogController:
  bool IsShowingAccountChooser() const override;
  void OnCancelDialog() override;
  void OnCloseDialog() override;
  void ResetDialog() override;

 protected:
  raw_ptr<PasswordsLeakDialogDelegate> getDelegate() const;
  password_manager::metrics_util::LeakDialogType getDialogType() const;

 private:
  const raw_ptr<PasswordsLeakDialogDelegate> delegate_;
  const password_manager::metrics_util::LeakDialogType dialog_type_;
  raw_ptr<CredentialLeakPrompt> credential_leak_dialog_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_BASE_CONTROLLER_H_
