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
    password_manager::LeakedPasswordDetails details,
    std::unique_ptr<LeakDialogMetricsRecorder> metrics_recorder)
    : delegate_(delegate),
      leak_dialog_traits_(CreateDialogTraits(details.leak_type)),
      url_(std::move(details.origin)),
      username_(std::move(details.username)),
      password_(std::move(details.password)),
      change_password_supported_(
          password_manager::IsPasswordChangeSupported(details.leak_type)),
      metrics_recorder_(std::move(metrics_recorder)) {}

CredentialLeakDialogControllerImpl::~CredentialLeakDialogControllerImpl() =
    default;

void CredentialLeakDialogControllerImpl::ShowCredentialLeakPrompt(
    std::unique_ptr<CredentialLeakPrompt> dialog) {
  DCHECK(dialog);
  credential_leak_dialog_ = std::move(dialog);
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
  credential_leak_dialog_.reset();
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
