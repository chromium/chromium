// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
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
    size_t action_index,
    const ElementFinder::Result& element,
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
      .Run(element, base::BindOnce(&PerformActionsSequentially,
                                   std::move(perform_actions), action_index + 1,
                                   element, std::move(done)));
}

void PerformAllImpl(std::unique_ptr<ElementActionVector> perform_actions,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> done) {
  PerformActionsSequentially(std::move(perform_actions), 0, element,
                             std::move(done), OkClientStatus());
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

template <typename R>
void RetainElementAndExecuteGetCallback(
    std::unique_ptr<ElementFinder::Result> element,
    base::OnceCallback<void(const ClientStatus&, const R&)> callback,
    const ClientStatus& status,
    const R& result) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status, result);
}

template <typename R>
void OnFindElementForGet(
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

template <typename R>
void FindElementAndGetImpl(
    const ActionDelegate* delegate,
    const Selector& selector,
    ElementActionGetCallback<R> perform_and_get,
    base::OnceCallback<void(const ClientStatus&, const R&)> done) {
  delegate->FindElement(
      selector, base::BindOnce(&OnFindElementForGet<R>,
                               std::move(perform_and_get), std::move(done)));
}

}  // namespace

void FindElementAndPerform(const ActionDelegate* delegate,
                           const Selector& selector,
                           ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(delegate, selector, std::move(perform),
                            std::move(done));
}

void FindElementAndPerformAll(
    const ActionDelegate* delegate,
    const Selector& selector,
    std::unique_ptr<ElementActionVector> perform_actions,
    base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(
      delegate, selector,
      base::BindOnce(&PerformAllImpl, std::move(perform_actions)),
      std::move(done));
}

void FindElementAndGetProperty(
    const ActionDelegate* delegate,
    const Selector& selector,
    ElementActionGetCallback<std::string> perform_and_get,
    base::OnceCallback<void(const ClientStatus&, const std::string&)> done) {
  FindElementAndGetImpl<std::string>(
      delegate, selector, std::move(perform_and_get), std::move(done));
}

void ClickOrTapElement(const ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       base::OnceCallback<void(const ClientStatus&)> done) {
  FindElementAndPerformImpl(
      delegate, selector,
      base::BindOnce(&PerformClickOrTapElement, delegate, click_type),
      std::move(done));
}
void PerformClickOrTapElement(
    const ActionDelegate* delegate,
    ClickType click_type,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> done) {
#ifdef NDEBUG
  VLOG(3) << __func__ << " click_type=" << click_type;
#else
  DVLOG(3) << __func__ << " click_type=" << click_type;
#endif

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::WaitForDocumentToBecomeInteractive,
                     delegate->GetWeakPtr()));
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::ScrollIntoView, delegate->GetWeakPtr()));
  actions->emplace_back(base::BindOnce(&ActionDelegate::ClickOrTapElement,
                                       delegate->GetWeakPtr(), click_type));

  PerformAllImpl(std::move(actions), element, std::move(done));
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
#ifdef NDEBUG
  VLOG(3) << __func__ << " focus: " << use_focus;
#else
  DVLOG(3) << __func__ << " focus: " << use_focus;
#endif

  auto actions = std::make_unique<ElementActionVector>();
  if (use_focus) {
    actions->emplace_back(
        base::BindOnce(&ActionDelegate::FocusField, delegate->GetWeakPtr()));
  } else {
    actions->emplace_back(
        base::BindOnce(&ActionDelegate::WaitForDocumentToBecomeInteractive,
                       delegate->GetWeakPtr()));
    actions->emplace_back(base::BindOnce(&ActionDelegate::ScrollIntoView,
                                         delegate->GetWeakPtr()));
    actions->emplace_back(base::BindOnce(&ActionDelegate::ClickOrTapElement,
                                         delegate->GetWeakPtr(),
                                         ClickType::CLICK));
  }
  actions->emplace_back(base::BindOnce(&ActionDelegate::SendKeyboardInput,
                                       delegate->GetWeakPtr(), codepoints,
                                       delay_in_millis));

  PerformAllImpl(std::move(actions), element, std::move(done));
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
        actions->emplace_back(
            base::BindOnce(&ActionDelegate::WaitForDocumentToBecomeInteractive,
                           delegate->GetWeakPtr()));
        actions->emplace_back(base::BindOnce(&ActionDelegate::ScrollIntoView,
                                             delegate->GetWeakPtr()));
        actions->emplace_back(base::BindOnce(&ActionDelegate::ClickOrTapElement,
                                             delegate->GetWeakPtr(),
                                             ClickType::CLICK));
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

  PerformAllImpl(std::move(actions), element, std::move(done));
}

}  // namespace action_delegate_util
}  // namespace autofill_assistant
