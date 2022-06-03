// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/check_option_element_action.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

CheckOptionElementAction::CheckOptionElementAction(ActionDelegate* delegate,
                                                   const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_check_option_element());
}

CheckOptionElementAction::~CheckOptionElementAction() = default;

void CheckOptionElementAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  ClientStatus select_status = delegate_->GetElementStore()->GetElement(
      proto_.check_option_element().select_id().identifier(), &select_);
  if (!select_status.ok()) {
    EndAction(select_status);
    return;
  }

  ClientStatus option_status = delegate_->GetElementStore()->GetElement(
      proto_.check_option_element().option_id().identifier(), &option_);
  if (!option_status.ok()) {
    EndAction(option_status);
    return;
  }

  delegate_->GetWebController()->CheckSelectedOptionElement(
      option_, select_,
      base::BindOnce(&CheckOptionElementAction::OnCheckSelectedOptionElement,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CheckOptionElementAction::OnCheckSelectedOptionElement(
    const ClientStatus& status) {
  processed_action_proto_->mutable_check_option_element_result()->set_match(
      status.ok());
  EndAction(proto_.check_option_element().mismatch_should_fail()
                ? status
                : OkClientStatus());
}

void CheckOptionElementAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
