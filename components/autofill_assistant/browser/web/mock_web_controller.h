// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_rect_getter.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/core/public/autofill_assistant_intent.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
class ElementFinderResult;
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
               void(std::unique_ptr<ElementFinderResult>,
                    const TopPadding&,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD5(ScrollIntoView,
               void(const std::string&,
                    const std::string&,
                    const std::string&,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD3(ScrollIntoViewIfNeeded,
               void(bool,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD2(CheckOnTop,
               void(const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD4(FillAddressForm,
               void(std::unique_ptr<autofill::AutofillProfile>,
                    const autofill_assistant::AutofillAssistantIntent intent,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD5(FillCardForm,
               void(std::unique_ptr<autofill::CreditCard>,
                    const autofill_assistant::AutofillAssistantIntent intent,
                    const std::u16string&,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD6(SelectOption,
               void(const std::string& re2,
                    bool case_sensitive,
                    SelectOptionProto::OptionComparisonAttribute
                        option_comparison_attribute,
                    bool strict,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(CheckSelectedOptionElement,
               void(const ElementFinderResult& option,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(SelectFieldValue,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(FocusField,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(SendKeyEvent,
               void(const KeyEvent& key_event,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD4(SendKeyboardInput,
               void(const std::vector<UChar32>& codepoints,
                    int delay_in_millisecond,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD4(SendTextInput,
               void(int key_press_delay_in_millisecond,
                    const std::string& value,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(GetOuterHtml,
               void(bool include_all_inner_text,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));
  MOCK_METHOD2(GetElementTag,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));
  MOCK_METHOD2(
      GetDocumentReadyState,
      void(const ElementFinderResult&,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>));

  MOCK_METHOD3(
      GetOuterHtmls,
      void(bool include_all_inner_text,
           const ElementFinderResult& elements,
           base::OnceCallback<void(const ClientStatus&,
                                   const std::vector<std::string>&)> callback));

  MOCK_METHOD4(WaitUntilElementIsStable,
               void(int,
                    base::TimeDelta wait_time,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            base::TimeDelta)> callback));
  MOCK_METHOD2(JsClickElement,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(ClickOrTapElement,
               void(ClickType click_type,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(GetFieldValue,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));
  MOCK_METHOD3(
      GetStringAttribute,
      void(const std::vector<std::string>&,
           const ElementFinderResult&,
           base::OnceCallback<void(const ClientStatus&, const std::string&)>));
  MOCK_METHOD3(SetValueAttribute,
               void(const std::string& value,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD4(SetAttribute,
               void(const std::vector<std::string>&,
                    const std::string&,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD1(GetVisualViewport,
               void(base::OnceCallback<void(const ClientStatus&, const RectF&)>
                        callback));
  MOCK_METHOD2(GetElementRect,
               void(const ElementFinderResult& element,
                    ElementRectGetter::ElementRectCallback callback));
  MOCK_METHOD3(WaitForDocumentReadyState,
               void(const ElementFinderResult& optional_frame_element,
                    DocumentReadyState min_ready_state,
                    base::OnceCallback<void(const ClientStatus&,
                                            DocumentReadyState,
                                            base::TimeDelta)> callback));
  MOCK_METHOD1(DispatchJsEvent,
               void(base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(ExecuteJS,
               void(const std::string& snippet,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  base::WeakPtr<WebController> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<MockWebController> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_
