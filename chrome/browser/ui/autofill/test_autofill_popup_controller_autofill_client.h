// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_POPUP_CONTROLLER_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_POPUP_CONTROLLER_AUTOFILL_CLIENT_H_

#include <concepts>
#include <memory>

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_view.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

// A modified `TestContentAutofillClient` that simulates the production behavior
// of the popup controller and popup view lifetimes on Desktop platforms.
template <typename Controller = AutofillSuggestionControllerForTest>
  requires(std::derived_from<Controller, AutofillPopupControllerImpl>)
class TestAutofillPopupControllerAutofillClient
    : public TestContentAutofillClient {
 public:
  explicit TestAutofillPopupControllerAutofillClient(
      content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    ON_CALL(*popup_view(), CreateSubPopupView)
        .WillByDefault(::testing::Return(sub_popup_view()->GetWeakPtr()));
  }

  ~TestAutofillPopupControllerAutofillClient() override { DoHide(); }

  // Returns the current controller. Controllers are specific to the `manager`'s
  // AutofillExternalDelegate. Therefore, when there are two consecutive
  // `popup_controller(x)` and `popup_controller(y)`, the second call hides the
  // old and creates new controller iff `x` and `y` are distinct.
  Controller& popup_controller(BrowserAutofillManagerForPopupTest& manager) {
    if (manager_of_last_controller_.get() != &manager) {
      DoHide();
      CHECK(!popup_controller_);
    }
    if (!popup_controller_) {
      popup_controller_ =
          (new Controller(manager.external_delegate().GetWeakPtrForTest(),
                          &GetWebContents(), gfx::RectF()))
              ->GetWeakPtr();
      test_api(cast_popup_controller()).SetView(popup_view_->GetWeakPtr());
      manager_of_last_controller_ = manager.GetWeakPtr();
      ON_CALL(cast_popup_controller(), Hide)
          .WillByDefault(
              [this](SuggestionHidingReason reason) { DoHide(reason); });
    }
    return cast_popup_controller();
  }

  MockAutofillPopupView* popup_view() { return popup_view_.get(); }

  MockAutofillPopupView* sub_popup_view() { return sub_popup_view_.get(); }

 private:
  void DoHide(SuggestionHidingReason reason) {
    if (popup_controller_) {
      cast_popup_controller().DoHide(reason);
    }
  }

  void DoHide() {
    if (popup_controller_) {
      cast_popup_controller().DoHide();
    }
  }

  Controller& cast_popup_controller() {
    return static_cast<Controller&>(*popup_controller_);
  }

  base::WeakPtr<AutofillSuggestionController> popup_controller_;
  base::WeakPtr<AutofillManager> manager_of_last_controller_;

  std::unique_ptr<MockAutofillPopupView> popup_view_ =
      std::make_unique<::testing::NiceMock<MockAutofillPopupView>>();
  std::unique_ptr<MockAutofillPopupView> sub_popup_view_ =
      std::make_unique<::testing::NiceMock<MockAutofillPopupView>>();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_POPUP_CONTROLLER_AUTOFILL_CLIENT_H_
