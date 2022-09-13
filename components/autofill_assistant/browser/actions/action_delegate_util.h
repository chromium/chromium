// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"

namespace autofill_assistant {
class ElementFinderResult;

namespace action_delegate_util {

// Finds the element given by the selector. If the resolution fails, it
// immediately executes the |done| callback. If the resolution succeeds, it
// executes the |perform| callback with the element and the |done| callback as
// arguments, while retaining the element.
void FindElementAndPerform(const ActionDelegate* delegate,
                           const Selector& selector,
                           element_action_util::ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done);

// Resolve the |text_value| and run the |perform| callback. Run the |done|
// callback with an error status if the |text_value| could not be resolved.
// Run the |done| callback with the result status of |perform| otherwise.
void PerformWithTextValue(
    const ActionDelegate* delegate,
    const TextValue& text_value,
    base::OnceCallback<void(const std::string&,
                            const ElementFinderResult&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done);

// Resolve the |client_id| and run the |perform| callback. Run the |done|
// callback with an error status if the |client_id| could not be resolved.
// Run the |done| callback with the result status of |perform| otherwise.
void PerformWithElementValue(
    const ActionDelegate* delegate,
    const ClientIdProto& client_id,
    base::OnceCallback<void(const ElementFinderResult&,
                            const ElementFinderResult&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done);

// Adds an optional step to the |actions|. If the step is |SKIP_STEP|, it does
// not add it. For |REPORT_STEP_RESULT| it adds the step ignoring a potential
// failure. For |REQUIRE_STEP_SUCCESS| it binds the step as is.
void AddOptionalStep(OptionalStep optional_step,
                     element_action_util::ElementActionCallback step,
                     element_action_util::ElementActionVector* actions);

// Adds a step to the |actions| and ignores its timing results.
void AddStepIgnoreTiming(
    base::OnceCallback<void(
        const ElementFinderResult&,
        base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>)> step,
    element_action_util::ElementActionVector* actions);

// Adds a step to the |actions| that does not have a callback.
void AddStepWithoutCallback(
    base::OnceCallback<void(const ElementFinderResult&)> step,
    element_action_util::ElementActionVector* actions);

void PerformClickOrTapElement(
    const ActionDelegate* delegate,
    ClickType click_type,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback);

void PerformSetFieldValue(
    const ActionDelegate* delegate,
    const std::string& value,
    KeyboardValueFillStrategy fill_strategy,
    int key_press_delay_in_millisecond,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback);

}  // namespace action_delegate_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_
