// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/release_elements_action.h"

#include "base/callback.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element_store.h"

namespace autofill_assistant {

ReleaseElementsAction::ReleaseElementsAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_release_elements());
}

ReleaseElementsAction::~ReleaseElementsAction() {}

void ReleaseElementsAction::InternalProcessAction(
    ProcessActionCallback callback) {
  process_action_callback_ = std::move(callback);

  for (const auto& client_id : proto_.release_elements().client_ids()) {
    delegate_->GetElementStore()->RemoveElement(client_id.identifier());
  }

  EndAction(ClientStatus(ACTION_APPLIED));
}

void ReleaseElementsAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
