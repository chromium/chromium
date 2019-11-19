// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/focus_element_action.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

FocusElementAction::FocusElementAction(ActionDelegate* delegate,
                                       const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_focus_element());
}

FocusElementAction::~FocusElementAction() {}

void FocusElementAction::InternalProcessAction(ProcessActionCallback callback) {
  const FocusElementProto& focus_element = proto_.focus_element();
  if (focus_element.has_title()) {
    // TODO(crbug.com/806868): Deprecate and remove message from this action and
    // use tell instead.
    delegate_->SetStatusMessage(focus_element.title());
  }
  Selector selector = Selector(focus_element.element()).MustBeVisible();
  if (selector.empty()) {
    DVLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  // Default value of 25%. This value should always be overriden
  // by backend.
  TopPadding top_padding{0.25, TopPadding::Unit::RATIO};
  switch (focus_element.top_padding().top_padding_case()) {
    case FocusElementProto::TopPadding::kPixels:
      top_padding = TopPadding(focus_element.top_padding().pixels(),
                               TopPadding::Unit::PIXELS);
      break;
    case FocusElementProto::TopPadding::kRatio:
      top_padding = TopPadding(focus_element.top_padding().ratio(),
                               TopPadding::Unit::RATIO);
      break;
    case FocusElementProto::TopPadding::TOP_PADDING_NOT_SET:
      // Default value set before switch.
      break;
  }

  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&FocusElementAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback), selector, top_padding));
}

void FocusElementAction::OnWaitForElement(ProcessActionCallback callback,
                                          const Selector& selector,
                                          const TopPadding& top_padding,
                                          const ClientStatus& element_status) {
  if (!element_status.ok()) {
    UpdateProcessedAction(element_status.proto_status());
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate_->FocusElement(
      selector, top_padding,
      base::BindOnce(&FocusElementAction::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FocusElementAction::OnFocusElement(ProcessActionCallback callback,
                                        const ClientStatus& status) {
  delegate_->SetTouchableElementArea(
      proto().focus_element().touchable_element_area());
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
