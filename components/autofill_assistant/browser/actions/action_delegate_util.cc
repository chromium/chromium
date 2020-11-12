// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
namespace action_delegate_util {
namespace {

void RetainElementAndExecuteCallback(
    std::unique_ptr<ElementFinder::Result> element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status);
}

void PerformActionsSequentially(
    std::unique_ptr<ElementActionVector> perform_actions,
    std::unique_ptr<ProcessedActionStatusDetailsProto> status_details,
    size_t action_index,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> done,
    const ClientStatus& status) {
  status_details->MergeFrom(status.details());

  if (!status.ok()) {
    VLOG(1) << __func__ << "Web-Action failed with status " << status;
    std::move(done).Run(ClientStatus(status.proto_status(), *status_details));
    return;
  }

  if (action_index >= perform_actions->size()) {
    std::move(done).Run(ClientStatus(status.proto_status(), *status_details));
    return;
  }

  std::move((*perform_actions)[action_index])
      .Run(element,
           base::BindOnce(&PerformActionsSequentially,
                          std::move(perform_actions), std::move(status_details),
                          action_index + 1, element, std::move(done)));
}

void OnFindElement(ElementActionCallback perform,
                   base::OnceCallback<void(const ClientStatus&)> done,
                   const ClientStatus& element_status,
                   std::unique_ptr<ElementFinder::Result> element_result) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find element.";
    std::move(done).Run(element_status);
    return;
  }

  std::move(perform).Run(
      *element_result,
      base::BindOnce(&RetainElementAndExecuteCallback,
                     std::move(element_result), std::move(done)));
}

void FindElementAndPerformImpl(
    const ActionDelegate* delegate,
    const Selector& selector,
    ElementActionCallback perform,
    base::OnceCallback<void(const ClientStatus&)> done) {
  delegate->FindElement(
      selector,
      base::BindOnce(&OnFindElement, std::move(perform), std::move(done)));
}

// Call |done| with a successful status, no matter what |status|.
//
// Note that the status details, if any, filled in |status| are conserved.
void IgnoreErrorStatus(base::OnceCallback<void(const ClientStatus&)> done,
                       const ClientStatus& status) {
  if (status.ok()) {
    std::move(done).Run(status);
    return;
  }
  std::move(done).Run(status.WithStatusOverride(ACTION_APPLIED));
}

// Execute [action] but skip any failures by transforming failed ClientStatus
// into successes.
//
// Note that the status details filled by the failed action are conserved.
void SkipIfFail(ElementActionCallback action,
                const ElementFinder::Result& element,
                base::OnceCallback<void(const ClientStatus&)> done) {
  std::move(action).Run(element,
                        base::BindOnce(&IgnoreErrorStatus, std::move(done)));
}

// Adds a sequence of actions that execute a click.
void AddClickOrTapSequence(const ActionDelegate* delegate,
                           ClickType click_type,
                           OptionalStep on_top,
                           ElementActionVector* actions) {
  actions->emplace_back(base::BindOnce(
      &ActionDelegate::WaitUntilDocumentIsInReadyState, delegate->GetWeakPtr(),
      delegate->GetSettings().document_ready_check_timeout,
      DOCUMENT_INTERACTIVE));
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::ScrollIntoView, delegate->GetWeakPtr()));
  if (click_type != ClickType::JAVASCRIPT) {
    actions->emplace_back(base::BindOnce(
        &ActionDelegate::WaitUntilElementIsStable, delegate->GetWeakPtr(),
        delegate->GetSettings().box_model_check_count,
        delegate->GetSettings().box_model_check_interval));
    AddOptionalStep(
        on_top,
        base::BindOnce(&ActionDelegate::CheckOnTop, delegate->GetWeakPtr()),
        actions);
  }
  actions->emplace_back(base::BindOnce(&ActionDelegate::ClickOrTapElement,
                                       delegate->GetWeakPtr(), click_type));
}

}  // namespace

void PerformAll(std::unique_ptr<ElementActionVector> perform_actions,
                const ElementFinder::Result& element,
                base::OnceCallback<void(const ClientStatus&)> done) {
  PerformActionsSequentially(
      std::move(perform_actions),
      std::make_unique<ProcessedActionStatusDetailsProto>(), 0, element,
      std::move(done), OkClientStatus());
}

void AddOptionalStep(OptionalStep optional_step,
                     ElementActionCallback step,
                     ElementActionVector* actions) {
  switch (optional_step) {
    case STEP_UNSPECIFIED:
      NOTREACHED() << __func__ << " unspecified optional_step";
      FALLTHROUGH;

    case SKIP_STEP:
      break;

    case REPORT_STEP_RESULT:
      actions->emplace_back(base::BindOnce(&SkipIfFail, std::move(step)));
      break;

    case REQUIRE_STEP_SUCCESS:
      actions->emplace_back(std::move(step));
      break;
  }
}

void FindElementAndPerform(const ActionDelegate* delegate,
                           const Selector& selector,
                           ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(delegate, selector, std::move(perform),
                            std::move(done));
}

void TakeElementAndPerform(ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done,
                           const ClientStatus& element_status,
                           std::unique_ptr<ElementFinder::Result> element) {
  OnFindElement(std::move(perform), std::move(done), element_status,
                std::move(element));
}

void ClickOrTapElement(const ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       OptionalStep on_top,
                       base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(
      delegate, selector,
      base::BindOnce(&PerformClickOrTapElement, delegate, click_type, on_top),
      std::move(done));
}

void PerformClickOrTapElement(
    const ActionDelegate* delegate,
    ClickType click_type,
    OptionalStep on_top,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  VLOG(3) << __func__ << " click_type=" << click_type << " on_top=" << on_top;

  auto actions = std::make_unique<ElementActionVector>();
  AddClickOrTapSequence(delegate, click_type, on_top, actions.get());
  PerformAll(std::move(actions), element, std::move(done));
}

void SendKeyboardInput(const ActionDelegate* delegate,
                       const Selector& selector,
                       const std::vector<UChar32> codepoints,
                       int delay_in_millis,
                       bool use_focus,
                       base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(
      delegate, selector,
      base::BindOnce(&PerformSendKeyboardInput, delegate, codepoints,
                     delay_in_millis, use_focus),
      std::move(done));
}

void PerformSendKeyboardInput(
    const ActionDelegate* delegate,
    const std::vector<UChar32> codepoints,
    int delay_in_millis,
    bool use_focus,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  VLOG(3) << __func__ << " focus: " << use_focus;

  auto actions = std::make_unique<ElementActionVector>();
  if (use_focus) {
    actions->emplace_back(
        base::BindOnce(&ActionDelegate::FocusField, delegate->GetWeakPtr()));
  } else {
    AddClickOrTapSequence(delegate, ClickType::CLICK, /* on_top=*/SKIP_STEP,
                          actions.get());
  }
  actions->emplace_back(base::BindOnce(&ActionDelegate::SendKeyboardInput,
                                       delegate->GetWeakPtr(), codepoints,
                                       delay_in_millis));

  PerformAll(std::move(actions), element, std::move(done));
}

void SetFieldValue(const ActionDelegate* delegate,
                   const Selector& selector,
                   const std::string& value,
                   KeyboardValueFillStrategy fill_strategy,
                   int key_press_delay_in_millisecond,
                   base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(
      delegate, selector,
      base::BindOnce(&PerformSetFieldValue, delegate, value, fill_strategy,
                     key_press_delay_in_millisecond),
      std::move(done));
}
void PerformSetFieldValue(const ActionDelegate* delegate,
                          const std::string& value,
                          KeyboardValueFillStrategy fill_strategy,
                          int key_press_delay_in_millisecond,
                          const ElementFinder::Result& element,
                          base::OnceCallback<void(const ClientStatus&)> done) {
#ifdef NDEBUG
  VLOG(3) << __func__ << " value=(redacted)"
          << ", strategy=" << fill_strategy;
#else
  DVLOG(3) << __func__ << " value=" << value << ", strategy=" << fill_strategy;
#endif

  auto actions = std::make_unique<ElementActionVector>();
  if (value.empty()) {
    actions->emplace_back(base::BindOnce(&ActionDelegate::SetValueAttribute,
                                         delegate->GetWeakPtr(),
                                         std::string()));
  } else {
    switch (fill_strategy) {
      case UNSPECIFIED_KEYBAORD_STRATEGY:
      case SET_VALUE:
        actions->emplace_back(base::BindOnce(&ActionDelegate::SetValueAttribute,
                                             delegate->GetWeakPtr(), value));
        break;
      case SIMULATE_KEY_PRESSES:
        actions->emplace_back(base::BindOnce(&ActionDelegate::SetValueAttribute,
                                             delegate->GetWeakPtr(),
                                             std::string()));
        AddClickOrTapSequence(delegate, ClickType::CLICK,
                              /* on_top= */ SKIP_STEP, actions.get());
        actions->emplace_back(base::BindOnce(
            &ActionDelegate::SendKeyboardInput, delegate->GetWeakPtr(),
            UTF8ToUnicode(value), key_press_delay_in_millisecond));
        break;
      case SIMULATE_KEY_PRESSES_SELECT_VALUE:
        // TODO(b/149004036): In case of empty, send a backspace (i.e. code 8),
        // instead of falling back to SetValueAttribute(""). This currently
        // fails in WebControllerBrowserTest.GetAndSetFieldValue. Fixing this
        // might fix b/148001624 as well.
        actions->emplace_back(base::BindOnce(&ActionDelegate::SelectFieldValue,
                                             delegate->GetWeakPtr()));
        actions->emplace_back(base::BindOnce(
            &ActionDelegate::SendKeyboardInput, delegate->GetWeakPtr(),
            UTF8ToUnicode(value), key_press_delay_in_millisecond));
        break;
      case SIMULATE_KEY_PRESSES_FOCUS:
        actions->emplace_back(base::BindOnce(&ActionDelegate::SetValueAttribute,
                                             delegate->GetWeakPtr(),
                                             std::string()));
        actions->emplace_back(base::BindOnce(&ActionDelegate::FocusField,
                                             delegate->GetWeakPtr()));
        actions->emplace_back(base::BindOnce(
            &ActionDelegate::SendKeyboardInput, delegate->GetWeakPtr(),
            UTF8ToUnicode(value), key_press_delay_in_millisecond));
        break;
    }
  }

  PerformAll(std::move(actions), element, std::move(done));
}

}  // namespace action_delegate_util
}  // namespace autofill_assistant
