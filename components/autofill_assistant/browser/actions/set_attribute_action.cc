// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_attribute_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

SetAttributeAction::SetAttributeAction(ActionDelegate* delegate,
                                       const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK_GT(proto_.set_attribute().element().selectors_size(), 0);
  DCHECK_GT(proto_.set_attribute().attribute_size(), 0);
}

SetAttributeAction::~SetAttributeAction() {}

void SetAttributeAction::InternalProcessAction(ProcessActionCallback callback) {
  Selector selector = Selector(proto_.set_attribute().element());
  if (selector.empty()) {
    DVLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }
  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&SetAttributeAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback), selector));
}

void SetAttributeAction::OnWaitForElement(ProcessActionCallback callback,
                                          const Selector& selector,
                                          const ClientStatus& element_status) {
  if (!element_status.ok()) {
    UpdateProcessedAction(element_status.proto_status());
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate_->SetAttribute(
      selector, ExtractVector(proto_.set_attribute().attribute()),
      proto_.set_attribute().value(),
      base::BindOnce(&SetAttributeAction::OnSetAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SetAttributeAction::OnSetAttribute(ProcessActionCallback callback,
                                        const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
