// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_cast_action.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

ShowCastAction::ShowCastAction(ActionDelegate* delegate,
                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_show_cast());
}

ShowCastAction::~ShowCastAction() {}

void ShowCastAction::InternalProcessAction(ProcessActionCallback callback) {
  const ShowCastProto& show_cast = proto_.show_cast();
  if (show_cast.has_title()) {
    // TODO(crbug.com/806868): Deprecate and remove message from this action and
    // use tell instead.
    delegate_->SetStatusMessage(show_cast.title());
  }
  Selector selector = Selector(show_cast.element_to_present()).MustBeVisible();
  if (selector.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  // Default value of 25%. This value should always be overridden by backend.
  TopPadding top_padding{0.25, TopPadding::Unit::RATIO};
  switch (show_cast.top_padding().top_padding_case()) {
    case ShowCastProto::TopPadding::kPixels:
      top_padding = TopPadding(show_cast.top_padding().pixels(),
                               TopPadding::Unit::PIXELS);
      break;
    case ShowCastProto::TopPadding::kRatio:
      top_padding =
          TopPadding(show_cast.top_padding().ratio(), TopPadding::Unit::RATIO);
      break;
    case ShowCastProto::TopPadding::TOP_PADDING_NOT_SET:
      // Default value set before switch.
      break;
  }

  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&ShowCastAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback), selector, top_padding));
}

void ShowCastAction::OnWaitForElement(ProcessActionCallback callback,
                                      const Selector& selector,
                                      const TopPadding& top_padding,
                                      const ClientStatus& element_status) {
  if (!element_status.ok()) {
    UpdateProcessedAction(element_status.proto_status());
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  auto actions = std::make_unique<action_delegate_util::ElementActionVector>();
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::WaitForDocumentToBecomeInteractive,
                     delegate_->GetWeakPtr()));
  actions->emplace_back(base::BindOnce(&ActionDelegate::ScrollToElementPosition,
                                       delegate_->GetWeakPtr(), selector,
                                       top_padding));
  action_delegate_util::FindElementAndPerformAll(
      delegate_, selector, std::move(actions),
      base::BindOnce(&ShowCastAction::OnScrollToElementPosition,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShowCastAction::OnScrollToElementPosition(ProcessActionCallback callback,
                                               const ClientStatus& status) {
  delegate_->SetTouchableElementArea(
      proto().show_cast().touchable_element_area());
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
