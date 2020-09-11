// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
namespace ActionDelegateUtil {
namespace {

void RetainElementAndExecuteCallback(
    std::unique_ptr<ElementFinder::Result> element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status);
}

void OnFindElement(
    base::OnceCallback<void(const ElementFinder::Result&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
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

void OnScrollIntoViewForClickOrTap(
    ActionDelegate* delegate,
    ClickType click_type,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& scroll_status) {
  if (!scroll_status.ok()) {
    VLOG(1) << __func__ << " Failed to scroll to the element.";
    std::move(callback).Run(scroll_status);
    return;
  }

  delegate->ClickOrTapElement(element, click_type, std::move(callback));
}

void OnWaitForDocumentToBecomeInteractiveForClickOrTap(
    ActionDelegate* delegate,
    ClickType click_type,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& wait_status) {
  if (!wait_status.ok()) {
    VLOG(1) << __func__
            << " Waiting for document to become interactive timed out.";
    std::move(callback).Run(wait_status);
    return;
  }

  delegate->ScrollIntoView(
      element, base::BindOnce(&OnScrollIntoViewForClickOrTap, delegate,
                              click_type, element, std::move(callback)));
}

void PerformClickOrTap(ActionDelegate* delegate,
                       ClickType click_type,
                       const ElementFinder::Result& element,
                       base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate->WaitForDocumentToBecomeInteractive(
      element,
      base::BindOnce(&OnWaitForDocumentToBecomeInteractiveForClickOrTap,
                     delegate, click_type, element, std::move(callback)));
}

void OnClickOrTapForSendKeyboardInput(
    ActionDelegate* delegate,
    const std::vector<UChar32> codepoints,
    int delay_in_millis,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& click_status) {
  if (!click_status.ok()) {
    std::move(callback).Run(click_status);
    return;
  }

  delegate->SendKeyboardInput(element, codepoints, delay_in_millis,
                              std::move(callback));
}

void PerformSendKeyboardInput(
    ActionDelegate* delegate,
    const std::vector<UChar32> codepoints,
    int delay_in_millis,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  PerformClickOrTap(
      delegate, ClickType::CLICK, element,
      base::BindOnce(&OnClickOrTapForSendKeyboardInput, delegate, codepoints,
                     delay_in_millis, element, std::move(callback)));
}

void PerformSetFieldValue(
    ActionDelegate* delegate,
    const std::string& value,
    KeyboardValueFillStrategy fill_strategy,
    int key_press_delay_in_millisecond,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  // TODO(b/158153191): This should reuse the callback chains in the util
  //  instead of relying on the |WebController| to properly implement it.
  //  This requires to extract more methods and some internal logic of
  //  |SetFieldValue|.
  delegate->SetFieldValue(element, value, fill_strategy,
                          key_press_delay_in_millisecond, std::move(callback));
}

}  // namespace

void FindElementAndPerform(
    ActionDelegate* delegate,
    const Selector& selector,
    base::OnceCallback<void(const ElementFinder::Result&,
                            base::OnceCallback<void(const ClientStatus&)>)>
        perform,
    base::OnceCallback<void(const ClientStatus&)> done) {
  DCHECK(!selector.empty());
  VLOG(3) << __func__ << " " << selector;
  delegate->FindElement(
      selector,
      base::BindOnce(&OnFindElement, std::move(perform), std::move(done)));
}

void ClickOrTapElement(ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       base::OnceCallback<void(const ClientStatus&)> callback) {
  FindElementAndPerform(
      delegate, selector,
      base::BindOnce(&PerformClickOrTap, delegate, click_type),
      std::move(callback));
}

void SendKeyboardInput(ActionDelegate* delegate,
                       const Selector& selector,
                       const std::vector<UChar32> codepoints,
                       int delay_in_millis,
                       base::OnceCallback<void(const ClientStatus&)> callback) {
  FindElementAndPerform(delegate, selector,
                        base::BindOnce(&PerformSendKeyboardInput, delegate,
                                       codepoints, delay_in_millis),
                        std::move(callback));
}

void SetFieldValue(ActionDelegate* delegate,
                   const Selector& selector,
                   const std::string& value,
                   KeyboardValueFillStrategy fill_strategy,
                   int key_press_delay_in_millisecond,
                   base::OnceCallback<void(const ClientStatus&)> callback) {
  FindElementAndPerform(
      delegate, selector,
      base::BindOnce(&PerformSetFieldValue, delegate, value, fill_strategy,
                     key_press_delay_in_millisecond),
      std::move(callback));
}

}  // namespace ActionDelegateUtil
}  // namespace autofill_assistant
