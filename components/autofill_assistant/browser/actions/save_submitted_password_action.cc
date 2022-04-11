// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/save_submitted_password_action.h"

#include <utility>
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"

using password_manager::PasswordChangeSuccessTracker;

namespace autofill_assistant {

// Default time out for a leak credential check
constexpr base::TimeDelta kLeakDetectionDefaultTimeout =
    base::Milliseconds(2000);

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

  // Set a flag in the response whether the submitted password is the same
  // as the currently saved one, so that scripts can retrigger the flow.
  // Always send a result even it is false. Otherwise, the result handler does
  // not realize that a previous result is stale and keeps it in memory.
  bool used_same_password =
      delegate_->GetWebsiteLoginManager()->SubmittedPasswordIsSame();
  processed_action_proto_->mutable_save_submitted_password_result()
      ->set_used_same_password(used_same_password);

  if (!used_same_password) {
    delegate_->GetWebsiteLoginManager()->SaveSubmittedPassword();
    delegate_->GetPasswordChangeSuccessTracker()->OnChangePasswordFlowCompleted(
        delegate_->GetUserData()->selected_login_->origin,
        delegate_->GetUserData()->selected_login_->username,
        PasswordChangeSuccessTracker::EndEvent::kAutomatedOwnPasswordFlow);
  }

  // If a timeout is specified, perform a leak check.
  if (proto_.save_submitted_password().has_leak_detection_timeout_ms()) {
    base::TimeDelta timeout = base::Milliseconds(
        proto_.save_submitted_password().leak_detection_timeout_ms());
    // If the specified timeout is zero, use the default.
    if (timeout.is_zero()) {
      timeout = kLeakDetectionDefaultTimeout;
    }
    delegate_->GetWebsiteLoginManager()
        ->CheckWhetherSubmittedCredentialIsLeaked(
            base::BindOnce(&SaveSubmittedPasswordAction::OnLeakCheckComplete,
                           weak_ptr_factory_.GetWeakPtr()),
            timeout);
    return;
  }

  EndAction(ClientStatus(ACTION_APPLIED));
}

void SaveSubmittedPasswordAction::OnLeakCheckComplete(
    LeakDetectionStatus status,
    bool is_leaked) {
  if (status.IsSuccess()) {
    processed_action_proto_->mutable_save_submitted_password_result()
        ->set_used_leaked_credential(is_leaked);
  } else {
    // TODO (crbug.com/1310169): Add proper logging/ UMA metrics.
    VLOG(1) << "SaveSubmittedPasswordAction: Leak check was not successful.";
  }
  EndAction(ClientStatus(ACTION_APPLIED));
}

void SaveSubmittedPasswordAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
