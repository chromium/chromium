// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;

CredentialLeakDialogControllerImpl::CredentialLeakDialogControllerImpl(
    PasswordsLeakDialogDelegate* delegate,
    CredentialLeakType leak_type,
    const GURL& origin)
    : delegate_(delegate), leak_type_(leak_type), origin_(origin) {}

CredentialLeakDialogControllerImpl::~CredentialLeakDialogControllerImpl() {
  ResetDialog();
}

void CredentialLeakDialogControllerImpl::ShowCredentialLeakPrompt(
    CredentialLeakPrompt* dialog) {
  DCHECK(dialog);
  credential_leak_dialog_ = dialog;
  credential_leak_dialog_->ShowCredentialLeakPrompt();
}

bool CredentialLeakDialogControllerImpl::IsShowingAccountChooser() const {
  return false;
}

void CredentialLeakDialogControllerImpl::OnCancelDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kClickedClose);
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogControllerImpl::OnAcceptDialog() {
  if (ShouldCheckPasswords()) {
    LogLeakDialogTypeAndDismissalReason(
        password_manager::GetLeakDialogType(leak_type_),
        LeakDialogDismissalReason::kClickedCheckPasswords);
    delegate_->NavigateToPasswordCheckup();
  } else {
    LogLeakDialogTypeAndDismissalReason(
        password_manager::GetLeakDialogType(leak_type_),
        LeakDialogDismissalReason::kClickedOk);
  }
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogControllerImpl::OnCloseDialog() {
  LogLeakDialogTypeAndDismissalReason(
      password_manager::GetLeakDialogType(leak_type_),
      LeakDialogDismissalReason::kNoDirectInteraction);
  delegate_->OnLeakDialogHidden();
}

base::string16 CredentialLeakDialogControllerImpl::GetAcceptButtonLabel()
    const {
  return password_manager::GetAcceptButtonLabel(leak_type_);
}

base::string16 CredentialLeakDialogControllerImpl::GetCancelButtonLabel()
    const {
  return password_manager::GetCancelButtonLabel();
}

base::string16 CredentialLeakDialogControllerImpl::GetDescription() const {
  return password_manager::GetDescription(leak_type_, origin_);
}

base::string16 CredentialLeakDialogControllerImpl::GetTitle() const {
  return password_manager::GetTitle(leak_type_);
}

bool CredentialLeakDialogControllerImpl::ShouldCheckPasswords() const {
  return password_manager::ShouldCheckPasswords(leak_type_);
}

bool CredentialLeakDialogControllerImpl::ShouldShowCancelButton() const {
  return password_manager::ShouldShowCancelButton(leak_type_);
}

void CredentialLeakDialogControllerImpl::ResetDialog() {
  if (credential_leak_dialog_) {
    credential_leak_dialog_->ControllerGone();
    credential_leak_dialog_ = nullptr;
  }
}
