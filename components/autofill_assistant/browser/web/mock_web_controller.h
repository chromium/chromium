// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_rect_getter.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
struct RectF;

class MockWebController : public WebController {
 public:
  MockWebController();
  ~MockWebController() override;

  MOCK_METHOD1(LoadURL, void(const GURL&));

  MOCK_METHOD3(FindElement,
               void(const Selector& selector,
                    bool strict,
                    ElementFinder::Callback callback));
  MOCK_METHOD4(ScrollToElementPosition,
               void(std::unique_ptr<ElementFinder::Result>,
                    const TopPadding&,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD5(ScrollIntoView,
               void(const std::string&,
                    const std::string&,
                    const std::string&,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD3(ScrollIntoViewIfNeeded,
               void(bool,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD2(CheckOnTop,
               void(const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD3(FillAddressForm,
               void(std::unique_ptr<autofill::AutofillProfile>,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD4(FillCardForm,
               void(std::unique_ptr<autofill::CreditCard>,
                    const std::u16string&,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD6(SelectOption,
               void(const std::string& re2,
                    bool case_sensitive,
                    SelectOptionProto::OptionComparisonAttribute
                        option_comparison_attribute,
                    bool strict,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(CheckSelectedOptionElement,
               void(const ElementFinder::Result& option,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(SelectFieldValue,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(FocusField,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(SendKeyEvent,
               void(const KeyEvent& key_event,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD4(SendKeyboardInput,
               void(const std::vector<UChar32>& codepoints,
                    int delay_in_millisecond,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD4(SendTextInput,
               void(int key_press_delay_in_millisecond,
                    const std::string& value,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(GetOuterHtml,
               void(bool include_all_inner_text,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));
  MOCK_METHOD2(GetElementTag,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));
  MOCK_METHOD2(
      GetDocumentReadyState,
      void(const ElementFinder::Result&,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>));

  MOCK_METHOD3(
      GetOuterHtmls,
      void(bool include_all_inner_text,
           const ElementFinder::Result& elements,
           base::OnceCallback<void(const ClientStatus&,
                                   const std::vector<std::string>&)> callback));

  MOCK_METHOD4(WaitUntilElementIsStable,
               void(int,
                    base::TimeDelta wait_time,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            base::TimeDelta)> callback));
  MOCK_METHOD2(JsClickElement,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(ClickOrTapElement,
               void(ClickType click_type,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(GetFieldValue,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));
  MOCK_METHOD3(
      GetStringAttribute,
      void(const std::vector<std::string>&,
           const ElementFinder::Result&,
           base::OnceCallback<void(const ClientStatus&, const std::string&)>));
  MOCK_METHOD3(SetValueAttribute,
               void(const std::string& value,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD4(SetAttribute,
               void(const std::vector<std::string>&,
                    const std::string&,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD1(GetVisualViewport,
               void(base::OnceCallback<void(const ClientStatus&, const RectF&)>
                        callback));
  MOCK_METHOD2(GetElementRect,
               void(const ElementFinder::Result& element,
                    ElementRectGetter::ElementRectCallback callback));
  MOCK_METHOD3(WaitForDocumentReadyState,
               void(const ElementFinder::Result& optional_frame_element,
                    DocumentReadyState min_ready_state,
                    base::OnceCallback<void(const ClientStatus&,
                                            DocumentReadyState,
                                            base::TimeDelta)> callback));
  MOCK_METHOD1(DispatchJsEvent,
               void(base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(ExecuteJS,
               void(const std::string& snippet,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  base::WeakPtr<WebController> GetWeakPtr() const override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<MockWebController> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_
