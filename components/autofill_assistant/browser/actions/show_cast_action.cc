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
#include "components/autofill_assistant/browser/client_settings.h"
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
  process_action_callback_ = std::move(callback);

  const ShowCastProto& show_cast = proto_.show_cast();
  if (show_cast.has_title()) {
    // TODO(crbug.com/806868): Deprecate and remove message from this action and
    // use tell instead.
    delegate_->SetStatusMessage(show_cast.title());
  }
  Selector selector = Selector(show_cast.element_to_present());
  if (selector.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
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
      selector, base::BindOnce(&ShowCastAction::OnWaitForElementTimed,
                               weak_ptr_factory_.GetWeakPtr(),
                               base::BindOnce(&ShowCastAction::OnWaitForElement,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              selector, top_padding)));
}

void ShowCastAction::OnWaitForElement(const Selector& selector,
                                      const TopPadding& top_padding,
                                      const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  auto actions = std::make_unique<action_delegate_util::ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ActionDelegate::WaitUntilDocumentIsInReadyState, delegate_->GetWeakPtr(),
      delegate_->GetSettings().document_ready_check_timeout,
      DOCUMENT_INTERACTIVE));
  auto wait_for_stable_element = proto_.show_cast().wait_for_stable_element();
  if (wait_for_stable_element == STEP_UNSPECIFIED) {
    wait_for_stable_element = SKIP_STEP;
  }
  action_delegate_util::AddOptionalStep(
      wait_for_stable_element,
      base::BindOnce(&ActionDelegate::WaitUntilElementIsStable,
                     delegate_->GetWeakPtr(),
                     proto_.show_cast().stable_check_max_rounds(),
                     base::TimeDelta::FromMilliseconds(
                         proto_.show_cast().stable_check_interval_ms())),
      actions.get());
  actions->emplace_back(base::BindOnce(&ActionDelegate::ScrollToElementPosition,
                                       delegate_->GetWeakPtr(), selector,
                                       top_padding));
  action_delegate_util::FindElementAndPerform(
      delegate_, selector,
      base::BindOnce(&action_delegate_util::PerformAll, std::move(actions)),
      base::BindOnce(&ShowCastAction::OnScrollToElementPosition,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShowCastAction::OnScrollToElementPosition(const ClientStatus& status) {
  delegate_->SetTouchableElementArea(
      proto().show_cast().touchable_element_area());
  EndAction(status);
}

void ShowCastAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
