// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

using password_manager::CreateDialogTraits;
using password_manager::CredentialLeakType;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogMetricsRecorder;

CredentialLeakDialogControllerImpl::CredentialLeakDialogControllerImpl(
    PasswordsLeakDialogDelegate* delegate,
    CredentialLeakType leak_type,
    const GURL& url,
    const std::u16string& username,
    std::unique_ptr<LeakDialogMetricsRecorder> metrics_recorder)
    : delegate_(delegate),
      leak_dialog_traits_(CreateDialogTraits(leak_type)),
      url_(url),
      username_(username),
      metrics_recorder_(std::move(metrics_recorder)) {}

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
  metrics_recorder_->LogLeakDialogTypeAndDismissalReason(
      LeakDialogDismissalReason::kClickedClose);
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogControllerImpl::OnAcceptDialog() {
  if (ShouldCheckPasswords()) {
    metrics_recorder_->LogLeakDialogTypeAndDismissalReason(
        LeakDialogDismissalReason::kClickedCheckPasswords);
    delegate_->NavigateToPasswordCheckup(
        password_manager::PasswordCheckReferrer::kPasswordBreachDialog);
  } else {
    metrics_recorder_->LogLeakDialogTypeAndDismissalReason(
        LeakDialogDismissalReason::kClickedOk);
  }
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogControllerImpl::OnCloseDialog() {
  metrics_recorder_->LogLeakDialogTypeAndDismissalReason(
      LeakDialogDismissalReason::kNoDirectInteraction);
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogControllerImpl::ResetDialog() {
  if (credential_leak_dialog_) {
    credential_leak_dialog_->ControllerGone();
    credential_leak_dialog_ = nullptr;
  }
}

std::u16string CredentialLeakDialogControllerImpl::GetAcceptButtonLabel()
    const {
  return leak_dialog_traits_->GetAcceptButtonLabel();
}

std::u16string CredentialLeakDialogControllerImpl::GetCancelButtonLabel()
    const {
  return leak_dialog_traits_->GetCancelButtonLabel();
}

std::u16string CredentialLeakDialogControllerImpl::GetDescription() const {
  return leak_dialog_traits_->GetDescription();
}

std::u16string CredentialLeakDialogControllerImpl::GetTitle() const {
  return leak_dialog_traits_->GetTitle();
}

bool CredentialLeakDialogControllerImpl::ShouldCheckPasswords() const {
  return leak_dialog_traits_->ShouldCheckPasswords();
}

bool CredentialLeakDialogControllerImpl::ShouldShowCancelButton() const {
  return leak_dialog_traits_->ShouldShowCancelButton();
}
