// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_password_reset_request_action.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"

namespace autofill_assistant {

RegisterPasswordResetRequestAction::RegisterPasswordResetRequestAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_register_password_reset_request());
}

RegisterPasswordResetRequestAction::~RegisterPasswordResetRequestAction() =
    default;

void RegisterPasswordResetRequestAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  if (!delegate_->GetUserData()->selected_login_) {
    VLOG(1) << "RegisterPasswordResetRequestAction: requested login details "
               "not available in client memory.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  delegate_->GetPasswordChangeSuccessTracker()->OnChangePasswordFlowModified(
      delegate_->GetUserData()->selected_login_->origin,
      delegate_->GetUserData()->selected_login_->username,
      password_manager::PasswordChangeSuccessTracker::StartEvent::
          kManualResetLinkFlow);

  EndAction(ClientStatus(ACTION_APPLIED));
}

void RegisterPasswordResetRequestAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
