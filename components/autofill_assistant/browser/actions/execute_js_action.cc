// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/execute_js_action.h"

#include "base/location.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

ExecuteJsAction::ExecuteJsAction(ActionDelegate* delegate,
                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_execute_js());
}

ExecuteJsAction::~ExecuteJsAction() = default;

void ExecuteJsAction::InternalProcessAction(ProcessActionCallback callback) {
  callback_ = std::move(callback);
  const std::string& client_id = proto_.execute_js().client_id().identifier();
  if (client_id.empty()) {
    EndAction(ClientStatus(INVALID_ACTION));
  }

  ClientStatus status =
      delegate_->GetElementStore()->GetElement(client_id, &element_);
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  if (proto_.execute_js().timeout_ms() > 0) {
    timer_.Start(FROM_HERE,
                 base::Milliseconds(proto_.execute_js().timeout_ms()),
                 base::BindOnce(&ExecuteJsAction::EndAction,
                                weak_ptr_factory_.GetWeakPtr(),
                                ClientStatus(TIMED_OUT)));
  }
  delegate_->GetWebController()->ExecuteJS(
      proto_.execute_js().js_snippet(), element_,
      base::BindOnce(&ExecuteJsAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExecuteJsAction::EndAction(const ClientStatus& status) {
  if (!callback_) {
    // Either the timer or the action already called here.
    return;
  }
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
