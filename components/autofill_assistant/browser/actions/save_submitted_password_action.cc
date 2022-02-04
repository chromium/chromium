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

  if (!delegate_->GetWebsiteLoginManager()->ReadyToCommitSubmittedPassword()) {
    VLOG(1) << "SaveSubmittedPasswordAction: no submitted password to save.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  delegate_->GetWebsiteLoginManager()->SaveSubmittedPassword();
  delegate_->GetPasswordChangeSuccessTracker()->OnChangePasswordFlowCompleted(
      delegate_->GetUserData()->selected_login_->origin,
      delegate_->GetUserData()->selected_login_->username,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedOwnPasswordFlow);

  EndAction(ClientStatus(ACTION_APPLIED));
}

void SaveSubmittedPasswordAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
