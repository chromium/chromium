// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "url/gurl.h"

class CredentialLeakPrompt;
class PasswordsLeakDialogDelegate;

// A UI controller responsible for the credential leak dialog.
class CredentialLeakDialogControllerImpl
    : public CredentialLeakDialogController {
 public:
  CredentialLeakDialogControllerImpl(
      PasswordsLeakDialogDelegate* delegate,
      password_manager::CredentialLeakType leak_type,
      const GURL& url,
      const std::u16string& username,
      std::unique_ptr<
          password_manager::metrics_util::LeakDialogMetricsRecorder>);

  CredentialLeakDialogControllerImpl(
      const CredentialLeakDialogControllerImpl&) = delete;
  CredentialLeakDialogControllerImpl& operator=(
      const CredentialLeakDialogControllerImpl&) = delete;

  ~CredentialLeakDialogControllerImpl() override;

  // Pop up the credential leak dialog.
  void ShowCredentialLeakPrompt(CredentialLeakPrompt* dialog);

  // CredentialLeakDialogController:
  bool IsShowingAccountChooser() const override;
  void OnCancelDialog() override;
  void OnAcceptDialog() override;
  void OnCloseDialog() override;
  void ResetDialog() override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetDescription() const override;
  std::u16string GetTitle() const override;
  bool ShouldCheckPasswords() const override;
  bool ShouldShowCancelButton() const override;

 private:
  raw_ptr<CredentialLeakPrompt> credential_leak_dialog_ = nullptr;
  raw_ptr<PasswordsLeakDialogDelegate> delegate_;
  std::unique_ptr<password_manager::LeakDialogTraits> leak_dialog_traits_;
  GURL url_;
  std::u16string username_;

  // Metrics recorder for leak dialog related UMA and UKM logging.
  std::unique_ptr<password_manager::metrics_util::LeakDialogMetricsRecorder>
      metrics_recorder_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_IMPL_H_
