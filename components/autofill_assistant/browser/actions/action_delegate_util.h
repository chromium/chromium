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
namespace ActionDelegateUtil {

// Finds the element given by the selector. If the resolution fails, it
// immediately executes the |done| callback. If the resolution succeeds, it
// executes the |perform| callback with the element and the |done| callback as
// arguments, while retaining the element.
void FindElementAndPerform(
    ActionDelegate* delegate,
    const Selector& selector,
    base::OnceCallback<void(const ElementFinder::Result&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
    base::OnceCallback<void(const ClientStatus&)> done);

void ClickOrTapElement(ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       base::OnceCallback<void(const ClientStatus&)> callback);

void SendKeyboardInput(ActionDelegate* delegate,
                       const Selector& selector,
                       const std::vector<UChar32> codepoints,
                       int delay_in_millis,
                       base::OnceCallback<void(const ClientStatus&)> callback);

void SetFieldValue(ActionDelegate* delegate,
                   const Selector& selector,
                   const std::string& value,
                   KeyboardValueFillStrategy fill_strategy,
                   int key_press_delay_in_millisecond,
                   base::OnceCallback<void(const ClientStatus&)> callback);

}  // namespace ActionDelegateUtil
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_UTIL_H_
