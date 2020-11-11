// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
namespace action_delegate_util {
namespace {

template <typename R>
void RetainElementAndExecuteGetCallback(
    std::unique_ptr<ElementFinder::Result> element,
    base::OnceCallback<void(const ClientStatus&, const R&)> callback,
    const ClientStatus& status,
    const R& result) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status, result);
}

}  // namespace

using ElementActionCallback =
    base::OnceCallback<void(const ElementFinder::Result&,
                            base::OnceCallback<void(const ClientStatus&)>)>;

using ElementActionVector = std::vector<ElementActionCallback>;

template <typename R>
using ElementActionGetCallback = base::OnceCallback<void(
    const ElementFinder::Result&,
    base::OnceCallback<void(const ClientStatus&, const R&)>)>;

// Finds the element given by the selector. If the resolution fails, it
// immediately executes the |done| callback. If the resolution succeeds, it
// executes the |perform| callback with the element and the |done| callback as
// arguments, while retaining the element.
void FindElementAndPerform(const ActionDelegate* delegate,
                           const Selector& selector,
                           ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done);

// Take ownership of the |element| and execute the |perform| callback with the
// element and the |done| callback as arguments, while retaining the element.
// If the initial status is not ok, execute the |done| callback immediately.
void TakeElementAndPerform(ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done,
                           const ClientStatus& element_status,
                           std::unique_ptr<ElementFinder::Result> element);

// Take ownership of the |element| and execute the |perform| callback with the
// element and the |done| callback as arguments, while retaining the element.
// If the initial status is not ok, execute the |done| callback with the default
// value immediately.
template <typename R>
void TakeElementAndGetProperty(
    ElementActionGetCallback<R> perform_and_get,
    base::OnceCallback<void(const ClientStatus&, const R&)> done,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find element.";
    std::move(done).Run(element_status, R());
    return;
  }

  std::move(perform_and_get)
      .Run(*element_result,
           base::BindOnce(&RetainElementAndExecuteGetCallback<R>,
                          std::move(element_result), std::move(done)));
}

void PerformAll(std::unique_ptr<ElementActionVector> perform_actions,
                const ElementFinder::Result& element,
                base::OnceCallback<void(const ClientStatus&)> done);

// Adds an optional step to the |actions|. If the step is |SKIP_STEP|, it does
// not add it. For |REPORT_STEP_RESULT| it adds the step ignoring a potential
// failure. For |REQUIRE_STEP_SUCCESS| it binds the step as is.
void AddOptionalStep(OptionalStep optional_step,
                     ElementActionCallback step,
                     ElementActionVector* actions);

void ClickOrTapElement(const ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       OptionalStep on_top,
                       base::OnceCallback<void(const ClientStatus&)> callback);
void PerformClickOrTapElement(
    const ActionDelegate* delegate,
    ClickType click_type,
    OptionalStep on_top,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback);

void SendKeyboardInput(const ActionDelegate* delegate,
                       const Selector& selector,
                       const std::vector<UChar32> codepoints,
                       int delay_in_millis,
                       bool use_focus,
                       base::OnceCallback<void(const ClientStatus&)> callback);
void PerformSendKeyboardInput(
    const ActionDelegate* delegate,
    const std::vector<UChar32> codepoints,
    int delay_in_millis,
    bool use_focus,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback);

void SetFieldValue(const ActionDelegate* delegate,
                   const Selector& selector,
                   const std::string& value,
                   KeyboardValueFillStrategy fill_strategy,
                   int key_press_delay_in_millisecond,
                   base::OnceCallback<void(const ClientStatus&)> callback);
void PerformSetFieldValue(
    const ActionDelegate* delegate,
    const std::string& value,
    KeyboardValueFillStrategy fill_strategy,
    int key_press_delay_in_millisecond,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback);

}  // namespace action_delegate_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_
