// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/save_generated_password_action.h"

#include <utility>
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"

using password_manager::PasswordChangeSuccessTracker;

namespace autofill_assistant {

SaveGeneratedPasswordAction::SaveGeneratedPasswordAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_save_generated_password());
}

SaveGeneratedPasswordAction::~SaveGeneratedPasswordAction() {}

void SaveGeneratedPasswordAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  auto save_password = proto_.save_generated_password();

  if (save_password.memory_key().empty()) {
    VLOG(1) << "SaveGeneratedPasswordAction: empty "
               "|client_memory_key|";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  if (!delegate_->GetUserData()->HasAdditionalValue(
          save_password.memory_key()) ||
      delegate_->GetUserData()
              ->GetAdditionalValue(save_password.memory_key())
              ->strings()
              .values()
              .size() != 1) {
    VLOG(1) << "SaveGeneratedPasswordAction: requested key '"
            << save_password.memory_key() << "' not available in client memory";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  if (!delegate_->GetWebsiteLoginManager()->ReadyToSaveGeneratedPassword()) {
    VLOG(1) << "SaveGeneratedPasswordAction: no generated password to save.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  delegate_->GetWebsiteLoginManager()->SaveGeneratedPassword();

  delegate_->GetPasswordChangeSuccessTracker()->OnChangePasswordFlowCompleted(
      delegate_->GetUserData()->selected_login_->origin,
      delegate_->GetUserData()->selected_login_->username,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen);

  EndAction(ClientStatus(ACTION_APPLIED));
}

void SaveGeneratedPasswordAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
