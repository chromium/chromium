// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/click_action.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
struct RectF;

class MockWebController : public WebController {
 public:
  MockWebController();
  ~MockWebController() override;

  MOCK_METHOD1(LoadURL, void(const GURL&));

  void ClickOrTapElement(
      const Selector& selector,
      ClickAction::ClickType click_type,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    // Transforming callback into a references allows using RunOnceCallback on
    // the argument.
    OnClickOrTapElement(selector, callback);
  }
  MOCK_METHOD2(OnClickOrTapElement,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void FocusElement(
      const Selector& selector,
      const TopPadding& top_padding,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnFocusElement(selector, top_padding, callback);
  }
  MOCK_METHOD3(OnFocusElement,
               void(const Selector& selector,
                    const TopPadding& top_padding,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void ElementCheck(
      const Selector& selector,
      bool strict,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnElementCheck(selector, callback);
  }
  MOCK_METHOD2(OnElementCheck,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void GetFieldValue(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback) override {
    OnGetFieldValue(selector, callback);
  }
  MOCK_METHOD2(OnGetFieldValue,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)>& callback));

  void GetVisualViewport(
      base::OnceCallback<void(bool, const RectF&)> callback) override {
    OnGetVisualViewport(callback);
  }
  MOCK_METHOD1(OnGetVisualViewport,
               void(base::OnceCallback<void(bool, const RectF&)>& callback));

  void GetElementPosition(
      const Selector& selector,
      base::OnceCallback<void(bool, const RectF&)> callback) override {
    OnGetElementPosition(selector, callback);
  }
  MOCK_METHOD2(OnGetElementPosition,
               void(const Selector& selector,
                    base::OnceCallback<void(bool, const RectF&)>& callback));

  void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) {
    OnWaitForWindowHeightChange(callback);
  }

  MOCK_METHOD1(OnWaitForWindowHeightChange,
               void(base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD2(
      OnGetDocumentReadyState,
      void(const Selector&,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>&));

  void GetDocumentReadyState(
      const Selector& frame,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) override {
    OnGetDocumentReadyState(frame, callback);
  }

  MOCK_METHOD3(
      OnWaitForDocumentReadyState,
      void(const Selector&,
           DocumentReadyState min_ready_state,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>&));

  void WaitForDocumentReadyState(
      const Selector& frame,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) override {
    OnWaitForDocumentReadyState(frame, min_ready_state, callback);
  }
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_WEB_CONTROLLER_H_
