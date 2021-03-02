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

  void FindElement(const Selector& selector,
                   bool strict_mode,
                   ElementFinder::Callback callback) override {
    OnFindElement(selector, callback);
  }
  MOCK_METHOD2(OnFindElement,
               void(const Selector& selector,
                    ElementFinder::Callback& callback));

  MOCK_METHOD2(ScrollIntoView,
               void(const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD2(CheckOnTop,
               void(const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD2(HighlightElement,
               void(const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>));
  MOCK_METHOD2(SelectFieldValue,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(FocusField,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(GetOuterHtml,
               void(const ElementFinder::Result& element,
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
  MOCK_METHOD2(
      GetOuterHtmls,
      void(const ElementFinder::Result& elements,
           base::OnceCallback<void(const ClientStatus&,
                                   const std::vector<std::string>&)> callback));

  MOCK_METHOD4(WaitUntilElementIsStable,
               void(const ElementFinder::Result& element,
                    int,
                    base::TimeDelta,
                    base::OnceCallback<void(const ClientStatus&,
                                            base::TimeDelta)> callback));

  void ClickOrTapElement(
      const ElementFinder::Result& element,
      ClickType click_type,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    // Transforming callback into a references allows using RunOnceCallback on
    // the argument.
    OnClickOrTapElement(element, callback);
  }
  MOCK_METHOD2(OnClickOrTapElement,
               void(const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void ScrollToElementPosition(
      std::unique_ptr<ElementFinder::Result> scrollable_element,
      const ElementFinder::Result& element,
      const TopPadding& top_padding,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnScrollToElementPosition(std::move(scrollable_element), element,
                              top_padding, callback);
  }
  MOCK_METHOD4(OnScrollToElementPosition,
               void(std::unique_ptr<ElementFinder::Result> scrollable_element,
                    const ElementFinder::Result& element,
                    const TopPadding& top_padding,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void GetFieldValue(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback) override {
    OnGetFieldValue(element, callback);
  }
  MOCK_METHOD2(OnGetFieldValue,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)>& callback));

  void GetStringAttribute(
      const ElementFinder::Result& element,
      const std::vector<std::string>& attributes,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback) override {
    OnGetStringAttribute(element, attributes, callback);
  }
  MOCK_METHOD3(OnGetStringAttribute,
               void(const ElementFinder::Result& element,
                    const std::vector<std::string>& attributes,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)>& callback));

  void GetVisualViewport(
      base::OnceCallback<void(const ClientStatus&, const RectF&)> callback)
      override {
    OnGetVisualViewport(callback);
  }
  MOCK_METHOD1(OnGetVisualViewport,
               void(base::OnceCallback<void(const ClientStatus&, const RectF&)>&
                        callback));

  void GetElementRect(
      const ElementFinder::Result& element,
      ElementRectGetter::ElementRectCallback callback) override {
    OnGetElementRect(element, callback);
  }
  MOCK_METHOD2(OnGetElementRect,
               void(const ElementFinder::Result& element,
                    ElementRectGetter::ElementRectCallback& callback));

  void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) {
    OnWaitForWindowHeightChange(callback);
  }

  MOCK_METHOD1(OnWaitForWindowHeightChange,
               void(base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD3(OnWaitForDocumentReadyState,
               void(const ElementFinder::Result&,
                    DocumentReadyState,
                    base::OnceCallback<void(const ClientStatus&,
                                            DocumentReadyState,
                                            base::TimeDelta)>&));

  void WaitForDocumentReadyState(
      const ElementFinder::Result& optional_frame_element,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback) override {
    OnWaitForDocumentReadyState(optional_frame_element, min_ready_state,
                                callback);
  }

  base::WeakPtr<WebController> GetWeakPtr() const override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<MockWebController> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_
