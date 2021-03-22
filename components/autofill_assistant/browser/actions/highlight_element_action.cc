// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/highlight_element_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

HighlightElementAction::HighlightElementAction(ActionDelegate* delegate,
                                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_highlight_element());
}

HighlightElementAction::~HighlightElementAction() {}

void HighlightElementAction::InternalProcessAction(
    ProcessActionCallback callback) {
  Selector selector = Selector(proto_.highlight_element().element());
  if (selector.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate_->ShortWaitForElementWithSlowWarning(
      selector,
      base::BindOnce(&HighlightElementAction::OnWaitForElementTimed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&HighlightElementAction::OnWaitForElement,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback), selector)));
}

void HighlightElementAction::OnWaitForElement(
    ProcessActionCallback callback,
    const Selector& selector,
    const ClientStatus& element_status) {
  if (!element_status.ok()) {
    UpdateProcessedAction(element_status.proto_status());
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  action_delegate_util::FindElementAndPerform(
      delegate_, selector,
      base::BindOnce(&WebController::HighlightElement,
                     delegate_->GetWebController()->GetWeakPtr()),
      base::BindOnce(&HighlightElementAction::OnHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HighlightElementAction::OnHighlightElement(ProcessActionCallback callback,
                                                const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
