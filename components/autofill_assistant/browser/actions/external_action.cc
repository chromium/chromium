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
    EndAction(ClientStatus(INVALID_ACTION));
  }

  auto external_action = proto_.external_action();

  SendActionInfo();
  if (external_action.allow_interrupt()) {
    delegate_->WaitForDom(
        /* max_wait_time= */ base::TimeDelta::Max(),
        /* allow_observer_mode = */ false, external_action.allow_interrupt(),
        /* observer= */ nullptr, /* check_elements= */ base::DoNothing(),
        /* callback= */ base::DoNothing());
  }
}

void ExternalAction::SendActionInfo() {
  delegate_->RequestExternalAction(
      proto_.external_action(),
      base::BindOnce(&ExternalAction::OnExternalActionFinished,
                     weak_ptr_factory_.GetWeakPtr()));
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
