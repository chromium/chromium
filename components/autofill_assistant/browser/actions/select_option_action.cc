// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/select_option_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

SelectOptionAction::SelectOptionAction(ActionDelegate* delegate,
                                       const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_select_option());
}

SelectOptionAction::~SelectOptionAction() {}

void SelectOptionAction::InternalProcessAction(ProcessActionCallback callback) {
  const SelectOptionProto& select_option = proto_.select_option();

  // A non prefilled |select_option| is not supported.
  if (!select_option.has_selected_option()) {
    DVLOG(1) << __func__ << ": empty option";
    UpdateProcessedAction(INVALID_ACTION);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }
  Selector selector = Selector(select_option.element());
  if (selector.empty()) {
    DVLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }
  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&SelectOptionAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback), selector));
}

void SelectOptionAction::OnWaitForElement(ProcessActionCallback callback,
                                          const Selector& selector,
                                          const ClientStatus& element_status) {
  if (!element_status.ok()) {
    UpdateProcessedAction(element_status.proto_status());
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate_->SelectOption(
      selector, proto_.select_option().selected_option(),
      base::BindOnce(&::autofill_assistant::SelectOptionAction::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SelectOptionAction::OnSelectOption(ProcessActionCallback callback,
                                        const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
