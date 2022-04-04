// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/save_submitted_password_action.h"

#include <utility>
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"

using password_manager::PasswordChangeSuccessTracker;

namespace autofill_assistant {

SaveSubmittedPasswordAction::SaveSubmittedPasswordAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {}

SaveSubmittedPasswordAction::~SaveSubmittedPasswordAction() {}

void SaveSubmittedPasswordAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  // If no password form has been submitted, fail.
  if (!delegate_->GetWebsiteLoginManager()->ReadyToSaveSubmittedPassword()) {
    VLOG(1) << "SaveSubmittedPasswordAction: no submitted password to save.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  // If a password form has been submitted, but no password update is
  // registered, then the new password must be the same as the previous one.
  // In that case, set a flag in the response so that scripts can retrigger
  // the flow.
  if (!delegate_->GetWebsiteLoginManager()->SubmittedPasswordIsSame()) {
    delegate_->GetWebsiteLoginManager()->SaveSubmittedPassword();
    delegate_->GetPasswordChangeSuccessTracker()->OnChangePasswordFlowCompleted(
        delegate_->GetUserData()->selected_login_->origin,
        delegate_->GetUserData()->selected_login_->username,
        PasswordChangeSuccessTracker::EndEvent::kAutomatedOwnPasswordFlow);
  } else {
    processed_action_proto_->mutable_save_submitted_password_result()
        ->set_used_same_password(true);
  }

  EndAction(ClientStatus(ACTION_APPLIED));
}

void SaveSubmittedPasswordAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
