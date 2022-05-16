// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/external_action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

ExternalAction::ExternalAction(ActionDelegate* delegate,
                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_external_action());
}

ExternalAction::~ExternalAction() = default;

void ExternalAction::InternalProcessAction(ProcessActionCallback callback) {
  callback_ = std::move(callback);
  if (!delegate_->SupportsExternalActions()) {
    VLOG(1) << "External action are not supported for this run.";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (!proto_.external_action().has_info()) {
    VLOG(1) << "The ExternalAction's |info| is missing.";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  delegate_->RequestExternalAction(
      proto_.external_action(),
      base::BindOnce(&ExternalAction::StartDomChecks,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ExternalAction::OnExternalActionFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  // Do not add any code here. External delegates may choose to end the action
  // immediately, which could result in *this being deleted and UaF errors for
  // code after the above call.
}

void ExternalAction::StartDomChecks() {
  const auto& external_action = proto_.external_action();
  if (external_action.allow_interrupt()) {
    delegate_->WaitForDom(
        /* max_wait_time= */ base::TimeDelta::Max(),
        /* allow_observer_mode = */ false, external_action.allow_interrupt(),
        /* observer= */ nullptr, /* check_elements= */ base::DoNothing(),
        /* callback= */ base::DoNothing());
  }
}

void ExternalAction::OnExternalActionFinished(
    ExternalActionDelegate::ActionResult result) {
  if (!callback_) {
    return;
  }

  EndAction(result.success ? ClientStatus(ACTION_APPLIED)
                           : ClientStatus(UNKNOWN_ACTION_STATUS));
}

void ExternalAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
