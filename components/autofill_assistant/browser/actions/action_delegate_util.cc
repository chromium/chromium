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

void RecursivePerformActions(
    const ElementFinder::Result& element,
    std::unique_ptr<ElementActionVector> perform_actions,
    size_t action_index,
    base::OnceCallback<void(const ClientStatus&)> done,
    const ClientStatus& status) {
  if (!status.ok()) {
    VLOG(1) << __func__ << "Web-Action failed with status " << status;
    std::move(done).Run(status);
    return;
  }

  if (action_index >= perform_actions->size()) {
    std::move(done).Run(status);
    return;
  }

  std::move((*perform_actions)[action_index])
      .Run(element, base::BindOnce(&RecursivePerformActions, element,
                                   std::move(perform_actions), action_index + 1,
                                   std::move(done)));
}

void OnFindElement(std::unique_ptr<ElementActionVector> perform_actions,
                   base::OnceCallback<void(const ClientStatus&)> done,
                   const ClientStatus& element_status,
                   std::unique_ptr<ElementFinder::Result> element_result) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find element.";
    std::move(done).Run(element_status);
    return;
  }

  DCHECK(!perform_actions->empty());
  RecursivePerformActions(
      *element_result, std::move(perform_actions), 0,
      base::BindOnce(&RetainElementAndExecuteCallback,
                     std::move(element_result), std::move(done)),
      OkClientStatus());
}

}  // namespace

void FindElementAndPerform(ActionDelegate* delegate,
                           const Selector& selector,
                           ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done) {
  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(std::move(perform));

  FindElementAndPerform(delegate, selector, std::move(actions),
                        std::move(done));
}

void FindElementAndPerform(ActionDelegate* delegate,
                           const Selector& selector,
                           std::unique_ptr<ElementActionVector> perform_actions,
                           base::OnceCallback<void(const ClientStatus&)> done) {
  DCHECK(!selector.empty());
  VLOG(3) << __func__ << " " << selector;
  delegate->FindElement(
      selector, base::BindOnce(&OnFindElement, std::move(perform_actions),
                               std::move(done)));
}

void ClickOrTapElement(ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       base::OnceCallback<void(const ClientStatus&)> callback) {
  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::WaitForDocumentToBecomeInteractive,
                     delegate->GetWeakPtr()));
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::ScrollIntoView, delegate->GetWeakPtr()));
  actions->emplace_back(base::BindOnce(&ActionDelegate::ClickOrTapElement,
                                       delegate->GetWeakPtr(), click_type));

  FindElementAndPerform(delegate, selector, std::move(actions),
                        std::move(callback));
}

void SendKeyboardInput(ActionDelegate* delegate,
                       const Selector& selector,
                       const std::vector<UChar32> codepoints,
                       int delay_in_millis,
                       base::OnceCallback<void(const ClientStatus&)> callback) {
  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::WaitForDocumentToBecomeInteractive,
                     delegate->GetWeakPtr()));
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::ScrollIntoView, delegate->GetWeakPtr()));
  actions->emplace_back(base::BindOnce(&ActionDelegate::ClickOrTapElement,
                                       delegate->GetWeakPtr(),
                                       ClickType::CLICK));
  actions->emplace_back(base::BindOnce(&ActionDelegate::SendKeyboardInput,
                                       delegate->GetWeakPtr(), codepoints,
                                       delay_in_millis));

  FindElementAndPerform(delegate, selector, std::move(actions),
                        std::move(callback));
}

void SetFieldValue(ActionDelegate* delegate,
                   const Selector& selector,
                   const std::string& value,
                   KeyboardValueFillStrategy fill_strategy,
                   int key_press_delay_in_millisecond,
                   base::OnceCallback<void(const ClientStatus&)> callback) {
  // TODO(b/158153191): This should reuse the callback chains in the util
  //  instead of relying on the |WebController| to properly implement it.
  //  This requires to extract more methods and some internal logic of
  //  |SetFieldValue|.
  FindElementAndPerform(
      delegate, selector,
      base::BindOnce(&ActionDelegate::SetFieldValue, delegate->GetWeakPtr(),
                     value, fill_strategy, key_press_delay_in_millisecond),
      std::move(callback));
}

}  // namespace ActionDelegateUtil
}  // namespace autofill_assistant
