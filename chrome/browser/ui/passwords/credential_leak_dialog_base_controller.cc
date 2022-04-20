// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_base_controller.h"

#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace {
using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogType;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;
}  // namespace

CredentialLeakDialogBaseController::CredentialLeakDialogBaseController(
    PasswordsLeakDialogDelegate* delegate,
    LeakDialogType dialog_type)
    : delegate_(delegate), dialog_type_(dialog_type) {}

CredentialLeakDialogBaseController::~CredentialLeakDialogBaseController() {
  ResetDialog();
}

void CredentialLeakDialogBaseController::ShowCredentialLeakPrompt(
    CredentialLeakPrompt* dialog) {
  DCHECK(dialog);
  credential_leak_dialog_ = dialog;
  credential_leak_dialog_->ShowCredentialLeakPrompt();
}

bool CredentialLeakDialogBaseController::IsShowingAccountChooser() const {
  return false;
}

void CredentialLeakDialogBaseController::OnCancelDialog() {
  LogLeakDialogTypeAndDismissalReason(dialog_type_,
                                      LeakDialogDismissalReason::kClickedClose);
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogBaseController::OnCloseDialog() {
  LogLeakDialogTypeAndDismissalReason(
      dialog_type_, LeakDialogDismissalReason::kNoDirectInteraction);
  delegate_->OnLeakDialogHidden();
}

void CredentialLeakDialogBaseController::ResetDialog() {
  if (credential_leak_dialog_) {
    credential_leak_dialog_->ControllerGone();
    credential_leak_dialog_ = nullptr;
  }
}

raw_ptr<PasswordsLeakDialogDelegate>
CredentialLeakDialogBaseController::getDelegate() const {
  return delegate_;
}

password_manager::metrics_util::LeakDialogType
CredentialLeakDialogBaseController::getDialogType() const {
  return dialog_type_;
}
