// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_AUTOFILL_CLIENT_H_

#include <concepts>
#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/android/access_loss/mock_password_access_loss_warning_bridge.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/mock_autofill_keyboard_accessory_view.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

// A modified `TestContentAutofillClient` that simulates the production behavior
// of the keyboard accessory controller and accessory view lifetimes on Android.
template <typename Controller = AutofillSuggestionControllerForTest>
  requires(
      std::derived_from<Controller, AutofillKeyboardAccessoryControllerImpl>)
class TestAutofillKeyboardAccessoryControllerAutofillClient
    : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;
  ~TestAutofillKeyboardAccessoryControllerAutofillClient() override {
    DoHide();
  }

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
                          &GetWebContents(), gfx::RectF(),
                          show_pwd_migration_warning_callback_.Get()))
              ->GetWeakPtr();
      test_api(cast_popup_controller())
          .SetView(std::make_unique<MockAutofillKeyboardAccessoryView>());
      test_api(cast_popup_controller())
          .SetAccessLossWarningBridge(
              std::make_unique<MockPasswordAccessLossWarningBridge>());
      manager_of_last_controller_ = manager.GetWeakPtr();
      ON_CALL(cast_popup_controller(), Hide)
          .WillByDefault(
              [this](SuggestionHidingReason reason) { DoHide(reason); });
    }
    return cast_popup_controller();
  }

  MockAutofillKeyboardAccessoryView* popup_view() {
    return popup_controller_ ? static_cast<MockAutofillKeyboardAccessoryView*>(
                                   test_api(cast_popup_controller()).view())
                             : nullptr;
  }

  base::MockCallback<typename Controller::ShowPasswordMigrationWarningCallback>&
  show_pwd_migration_warning_callback() {
    return show_pwd_migration_warning_callback_;
  }

  MockPasswordAccessLossWarningBridge* access_loss_warning_bridge() {
    return popup_controller_
               ? static_cast<MockPasswordAccessLossWarningBridge*>(
                     test_api(cast_popup_controller())
                         .access_loss_warning_bridge())
               : nullptr;
  }

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

  base::MockCallback<typename Controller::ShowPasswordMigrationWarningCallback>
      show_pwd_migration_warning_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_AUTOFILL_CLIENT_H_
