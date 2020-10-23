// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
namespace action_delegate_util {

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

// Finds the element given by the selector. If the resolution fails, it
// immediately executes the |done| callback. If the resolution succeeds, it
// executes the |perform_actions| callbacks in sequence with the element and
// the |done| callback as arguments, while retaining the element.
void FindElementAndPerformAll(
    const ActionDelegate* delegate,
    const Selector& selector,
    std::unique_ptr<ElementActionVector> perform_actions,
    base::OnceCallback<void(const ClientStatus&)> done);

// Finds the element given by the selector. If the resolution fails, it
// immediately executes the |done| callback with the default value.
// If the resolution succeeds, it executes the |perform_and_get| callback with
// the element and the |done| callback as arguments, while retaining the
// element.
void FindElementAndGetProperty(
    const ActionDelegate* delegate,
    const Selector& selector,
    ElementActionGetCallback<std::string> perform_and_get,
    base::OnceCallback<void(const ClientStatus&, const std::string&)> done);

void ClickOrTapElement(const ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       base::OnceCallback<void(const ClientStatus&)> callback);
void PerformClickOrTapElement(
    const ActionDelegate* delegate,
    ClickType click_type,
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
