// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace autofill_assistant {
namespace action_delegate_util {
namespace {

void RetainElementAndExecuteCallback(
    std::unique_ptr<ElementFinderResult> element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status);
}

void FindElementAndPerformImpl(
    const ActionDelegate* delegate,
    const Selector& selector,
    element_action_util::ElementActionCallback perform,
    base::OnceCallback<void(const ClientStatus&)> done) {
  delegate->FindElement(
      selector, base::BindOnce(&element_action_util::TakeElementAndPerform,
                               std::move(perform), std::move(done)));
}

// Call |done| with the |status| while ignoring the |wait_time|.
void IgnoreTimingResult(base::OnceCallback<void(const ClientStatus&)> done,
                        const ClientStatus& status,
                        base::TimeDelta wait_time) {
  std::move(done).Run(status);
}

// Execute |action| and ignore the timing result.
void RunAndIgnoreTiming(
    base::OnceCallback<void(
        const ElementFinderResult&,
        base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>)> action,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  std::move(action).Run(element,
                        base::BindOnce(&IgnoreTimingResult, std::move(done)));
}

void RunAndCallSuccessCallback(
    base::OnceCallback<void(const ElementFinderResult&)> step,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  std::move(step).Run(element);
  std::move(done).Run(OkClientStatus());
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

// Execute |action| but skip any failures by transforming failed ClientStatus
// into successes.
//
// Note that the status details filled by the failed action are conserved.
void SkipIfFail(element_action_util::ElementActionCallback action,
                const ElementFinderResult& element,
                base::OnceCallback<void(const ClientStatus&)> done) {
  std::move(action).Run(element,
                        base::BindOnce(&IgnoreErrorStatus, std::move(done)));
}

// Adds a sequence of actions that execute a click.
void AddClickOrTapSequence(const ActionDelegate* delegate,
                           ClickType click_type,
                           element_action_util::ElementActionVector* actions) {
  AddStepIgnoreTiming(
      base::BindOnce(&ActionDelegate::WaitUntilDocumentIsInReadyState,
                     delegate->GetWeakPtr(),
                     delegate->GetSettings().document_ready_check_timeout,
                     DOCUMENT_INTERACTIVE),
      actions);
  actions->emplace_back(base::BindOnce(
      &WebController::ScrollIntoView,
      delegate->GetWebController()->GetWeakPtr(),
      /* animation= */ std::string(), /* vertical_alignment= */ "center",
      /* horizontal_alignment= */ "center"));
  if (click_type == ClickType::JAVASCRIPT) {
    actions->emplace_back(
        base::BindOnce(&WebController::JsClickElement,
                       delegate->GetWebController()->GetWeakPtr()));
  } else {
    AddStepIgnoreTiming(
        base::BindOnce(&WebController::WaitUntilElementIsStable,
                       delegate->GetWebController()->GetWeakPtr(),
                       delegate->GetSettings().box_model_check_count,
                       delegate->GetSettings().box_model_check_interval),
        actions);
    actions->emplace_back(
        base::BindOnce(&WebController::ClickOrTapElement,
                       delegate->GetWebController()->GetWeakPtr(), click_type));
  }
}

void OnResolveTextValue(
    base::OnceCallback<void(const std::string&,
                            const ElementFinderResult&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done,
    const ClientStatus& status,
    const std::string& value) {
  if (!status.ok()) {
    std::move(done).Run(status);
    return;
  }
  std::move(perform).Run(value, element, std::move(done));
}

}  // namespace

void PerformWithTextValue(
    const ActionDelegate* delegate,
    const TextValue& text_value,
    base::OnceCallback<void(const std::string&,
                            const ElementFinderResult&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  user_data::ResolveTextValue(
      text_value, element, delegate,
      base::BindOnce(&OnResolveTextValue, std::move(perform), element,
                     std::move(done)));
}

void PerformWithElementValue(
    const ActionDelegate* delegate,
    const ClientIdProto& client_id,
    base::OnceCallback<void(const ElementFinderResult&,
                            const ElementFinderResult&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  std::unique_ptr<ElementFinderResult> element_result =
      std::make_unique<ElementFinderResult>();
  ElementFinderResult* element_result_ptr = element_result.get();
  ClientStatus element_status = delegate->GetElementStore()->GetElement(
      client_id.identifier(), element_result_ptr);
  if (!element_status.ok()) {
    std::move(done).Run(element_status);
    return;
  }

  std::move(perform).Run(
      *element_result_ptr, element,
      base::BindOnce(&RetainElementAndExecuteCallback,
                     std::move(element_result), std::move(done)));
}

void AddOptionalStep(OptionalStep optional_step,
                     element_action_util::ElementActionCallback step,
                     element_action_util::ElementActionVector* actions) {
  switch (optional_step) {
    case STEP_UNSPECIFIED:
      NOTREACHED() << __func__ << " unspecified optional_step";
      [[fallthrough]];

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

void AddStepIgnoreTiming(
    base::OnceCallback<void(
        const ElementFinderResult&,
        base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>)> step,
    element_action_util::ElementActionVector* actions) {
  actions->emplace_back(base::BindOnce(&RunAndIgnoreTiming, std::move(step)));
}

void AddStepWithoutCallback(
    base::OnceCallback<void(const ElementFinderResult&)> step,
    element_action_util::ElementActionVector* actions) {
  actions->emplace_back(
      base::BindOnce(&RunAndCallSuccessCallback, std::move(step)));
}

void FindElementAndPerform(const ActionDelegate* delegate,
                           const Selector& selector,
                           element_action_util::ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(delegate, selector, std::move(perform),
                            std::move(done));
}

void PerformClickOrTapElement(
    const ActionDelegate* delegate,
    ClickType click_type,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
  VLOG(3) << __func__ << " click_type=" << click_type;

  auto actions = std::make_unique<element_action_util::ElementActionVector>();
  AddClickOrTapSequence(delegate, click_type, actions.get());
  element_action_util::PerformAll(std::move(actions), element, std::move(done));
}

void PerformSetFieldValue(const ActionDelegate* delegate,
                          const std::string& value,
                          KeyboardValueFillStrategy fill_strategy,
                          int key_press_delay_in_millisecond,
                          const ElementFinderResult& element,
                          base::OnceCallback<void(const ClientStatus&)> done) {
#ifdef NDEBUG
  VLOG(3) << __func__ << " value=(redacted)"
          << ", strategy=" << fill_strategy;
#else
  DVLOG(3) << __func__ << " value=" << value << ", strategy=" << fill_strategy;
#endif

  auto actions = std::make_unique<element_action_util::ElementActionVector>();
  if (value.empty()) {
    actions->emplace_back(base::BindOnce(
        &WebController::SetValueAttribute,
        delegate->GetWebController()->GetWeakPtr(), std::string()));
  } else {
    switch (fill_strategy) {
      case UNSPECIFIED_KEYBAORD_STRATEGY:
      case SET_VALUE:
        actions->emplace_back(
            base::BindOnce(&WebController::SetValueAttribute,
                           delegate->GetWebController()->GetWeakPtr(), value));
        break;
      case SIMULATE_KEY_PRESSES:
        actions->emplace_back(base::BindOnce(
            &WebController::SetValueAttribute,
            delegate->GetWebController()->GetWeakPtr(), std::string()));
        AddClickOrTapSequence(delegate, ClickType::CLICK, actions.get());
        actions->emplace_back(base::BindOnce(
            &WebController::SendKeyboardInput,
            delegate->GetWebController()->GetWeakPtr(), UTF8ToUnicode(value),
            key_press_delay_in_millisecond));
        break;
      case SIMULATE_KEY_PRESSES_SELECT_VALUE: {
        actions->emplace_back(
            base::BindOnce(&WebController::SelectFieldValue,
                           delegate->GetWebController()->GetWeakPtr()));
        KeyEvent clear_event;
        clear_event.add_command("SelectAll");
        clear_event.add_command("DeleteBackward");
        clear_event.set_key(
            ui::KeycodeConverter::DomKeyToKeyString(ui::DomKey::BACKSPACE));
        actions->emplace_back(base::BindOnce(
            &WebController::SendKeyEvent,
            delegate->GetWebController()->GetWeakPtr(), clear_event));
        actions->emplace_back(base::BindOnce(
            &WebController::SendKeyboardInput,
            delegate->GetWebController()->GetWeakPtr(), UTF8ToUnicode(value),
            key_press_delay_in_millisecond));
        break;
      }
      case SIMULATE_KEY_PRESSES_FOCUS:
        actions->emplace_back(base::BindOnce(
            &WebController::SetValueAttribute,
            delegate->GetWebController()->GetWeakPtr(), std::string()));
        actions->emplace_back(
            base::BindOnce(&WebController::FocusField,
                           delegate->GetWebController()->GetWeakPtr()));
        actions->emplace_back(base::BindOnce(
            &WebController::SendKeyboardInput,
            delegate->GetWebController()->GetWeakPtr(), UTF8ToUnicode(value),
            key_press_delay_in_millisecond));
        break;
    }
  }

  element_action_util::PerformAll(std::move(actions), element, std::move(done));
}

}  // namespace action_delegate_util
}  // namespace autofill_assistant
