// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/perform_on_single_element_action.h"

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/web/element_store.h"

namespace autofill_assistant {

PerformOnSingleElementAction::PerformOnSingleElementAction(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    bool element_is_optional,
    PerformAction perform,
    PerformTimedAction perform_timed)
    : Action(delegate, proto),
      client_id_(client_id.identifier()),
      element_is_optional_(element_is_optional),
      perform_(std::move(perform)),
      perform_timed_(std::move(perform_timed)) {}

PerformOnSingleElementAction::~PerformOnSingleElementAction() = default;

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithClientId(ActionDelegate* delegate,
                                           const ActionProto& proto,
                                           const ClientIdProto& client_id,
                                           PerformAction perform) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ false, std::move(perform),
      /* perform_timed= */ base::NullCallback()));
}

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithOptionalClientId(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    PerformAction perform) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ true, std::move(perform),
      /* perform_timed= */ base::NullCallback()));
}

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithClientIdTimed(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    PerformTimedAction perform_timed) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ false,
      /* perform= */ base::NullCallback(), std::move(perform_timed)));
}

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithOptionalClientIdTimed(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    PerformTimedAction perform_timed) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ true,
      /* perform= */ base::NullCallback(), std::move(perform_timed)));
}

void PerformOnSingleElementAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  if (client_id_.empty() && !element_is_optional_) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  if (!client_id_.empty()) {
    ClientStatus status =
        delegate_->GetElementStore()->GetElement(client_id_, &element_);
    if (!status.ok()) {
      EndAction(status);
      return;
    }
  }

  if (perform_) {
    std::move(perform_).Run(
        element_, base::BindOnce(&PerformOnSingleElementAction::EndAction,
                                 weak_ptr_factory_.GetWeakPtr()));
    return;
  } else if (perform_timed_) {
    std::move(perform_timed_)
        .Run(element_,
             base::BindOnce(
                 &PerformOnSingleElementAction::OnWaitForElementTimed,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::BindOnce(&PerformOnSingleElementAction::EndAction,
                                weak_ptr_factory_.GetWeakPtr())));
    return;
  }
  NOTREACHED();
  EndAction(ClientStatus(INVALID_ACTION));
}

void PerformOnSingleElementAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
