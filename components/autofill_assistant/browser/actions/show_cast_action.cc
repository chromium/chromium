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
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

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

  if (proto_.show_cast().has_container()) {
    Selector container_selector = Selector(proto_.show_cast().container());
    if (container_selector.empty()) {
      VLOG(1) << __func__ << ": empty selector for container";
      EndAction(ClientStatus(INVALID_SELECTOR));
      return;
    }
    delegate_->FindElement(
        container_selector,
        base::BindOnce(&ShowCastAction::OnFindContainer,
                       weak_ptr_factory_.GetWeakPtr(), selector, top_padding));
  } else {
    ScrollToElement(selector, top_padding, /* container= */ nullptr);
  }
}

void ShowCastAction::OnFindContainer(
    const Selector& selector,
    const TopPadding& top_padding,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinderResult> container) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find container.";
    EndAction(element_status);
    return;
  }

  ScrollToElement(selector, top_padding, std::move(container));
}

void ShowCastAction::ScrollToElement(
    const Selector& selector,
    const TopPadding& top_padding,
    std::unique_ptr<ElementFinderResult> container) {
  auto actions = std::make_unique<element_action_util::ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ShowCastAction::RunAndIncreaseWaitTimer, weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&ActionDelegate::WaitUntilDocumentIsInReadyState,
                     delegate_->GetWeakPtr(),
                     delegate_->GetSettings().document_ready_check_timeout,
                     DOCUMENT_INTERACTIVE)));
  auto wait_for_stable_element = proto_.show_cast().wait_for_stable_element();
  if (wait_for_stable_element == STEP_UNSPECIFIED) {
    wait_for_stable_element = SKIP_STEP;
  }
  action_delegate_util::AddOptionalStep(
      wait_for_stable_element,
      base::BindOnce(&WebController::ScrollIntoViewIfNeeded,
                     delegate_->GetWebController()->GetWeakPtr(),
                     /* center= */ true),
      actions.get());
  action_delegate_util::AddOptionalStep(
      wait_for_stable_element,
      base::BindOnce(
          &ShowCastAction::RunAndIncreaseWaitTimer,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&WebController::WaitUntilElementIsStable,
                         delegate_->GetWebController()->GetWeakPtr(),
                         proto_.show_cast().stable_check_max_rounds(),
                         base::Milliseconds(
                             proto_.show_cast().stable_check_interval_ms()))),
      actions.get());
  action_delegate_util::AddStepWithoutCallback(
      base::BindOnce(&ActionDelegate::StoreScrolledToElement,
                     delegate_->GetWeakPtr()),
      actions.get());
  actions->emplace_back(
      base::BindOnce(&WebController::ScrollToElementPosition,
                     delegate_->GetWebController()->GetWeakPtr(),
                     std::move(container), top_padding));

  action_delegate_util::FindElementAndPerform(
      delegate_, selector,
      base::BindOnce(&element_action_util::PerformAll, std::move(actions)),
      base::BindOnce(&ShowCastAction::OnScrollToElementPosition,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShowCastAction::RunAndIncreaseWaitTimer(
    base::OnceCallback<void(
        const ElementFinderResult&,
        base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>)> action,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  std::move(action).Run(
      element, base::BindOnce(&ShowCastAction::OnWaitForElementTimed,
                              weak_ptr_factory_.GetWeakPtr(), std::move(done)));
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
