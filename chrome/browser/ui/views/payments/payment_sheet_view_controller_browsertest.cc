// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/metrics.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/views_switches.h"

namespace payments {

class PaymentSheetViewControllerTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentSheetViewControllerTest() = default;
  ~PaymentSheetViewControllerTest() override = default;

  void OnPayCalled() override { pay_was_called_ = true; }

 protected:
  bool pay_was_called_ = false;
};

// The [Continue] button should be protected against accidental inputs on
// initial show and have protection restarted when revealed after the processing
// spinner or widget visibility changes.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerTest,
                       ContinueButtonIgnoresAccidentalInputs) {
  // `PaymentRequestBrowserTestBase::SetUpCommandLine()` disables input event
  // activation protection by default so other tests can click buttons
  // immediately. Remove the switch here to test the real input protector.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      views::switches::kDisableInputEventActivationProtectionForTesting);

  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  ASSERT_TRUE(IsViewVisible(DialogViewID::PAY_BUTTON));
  ASSERT_TRUE(IsViewVisible(DialogViewID::CANCEL_BUTTON));
  ASSERT_TRUE(IsPayButtonEnabled());
  ASSERT_FALSE(pay_was_called_);

  views::View* sheet_view =
      GetByDialogViewID(DialogViewID::PAYMENT_REQUEST_SHEET);
  auto* controller = static_cast<PaymentSheetViewController*>(
      test_api(dialog_view()).controller_map()->at(sheet_view).get());
  views::View* button_view = GetByDialogViewID(DialogViewID::PAY_BUTTON);
  ASSERT_TRUE(button_view);

  // Immediately after initial show, key inputs within the cooldown window are
  // ignored (verifying `allow_key_events = false`).
  views::test::ButtonTestApi(static_cast<views::Button*>(button_view))
      .NotifyClick(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                                ui::EF_NONE));
  EXPECT_FALSE(pay_was_called_);

  controller->input_protector_for_testing()->ResetForTesting();

  // Covering the sheet with the processing spinner and then revealing it
  // should restart the input protection window.
  ResetEventWaiter(DialogEvent::PROCESSING_SPINNER_SHOWN);
  dialog_view()->ShowProcessingSpinner();
  ASSERT_TRUE(WaitForObservedEvent());

  ResetEventWaiter(DialogEvent::PROCESSING_SPINNER_HIDDEN);
  dialog_view()->HideProcessingSpinner();
  ASSERT_TRUE(WaitForObservedEvent());

  ClickOnDialogView(button_view);
  EXPECT_FALSE(pay_was_called_);

  controller->input_protector_for_testing()->ResetForTesting();

  // Hiding and re-showing the dialog widget (e.g. across a tab switch) should
  // also restart the input protection window.
  dialog_view()->GetWidget()->Hide();
  dialog_view()->GetWidget()->Show();

  ClickOnDialogView(button_view);
  EXPECT_FALSE(pay_was_called_);

  // A click after the cooldown interval has elapsed should succeed.
  views::test::ButtonTestApi(static_cast<views::Button*>(button_view))
      .NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
          ui::EventTimeForNow() + views::GetDoubleClickInterval(),
          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(pay_was_called_);
}

// The 'Continue' or 'Cancel' buttons should not be auto-focused; see
// https://crbug.com/40062377
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerTest,
                       ContinueIsNotAutoFocused) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  EXPECT_TRUE(IsViewVisible(DialogViewID::PAY_BUTTON));
  EXPECT_TRUE(IsViewVisible(DialogViewID::CANCEL_BUTTON));
  EXPECT_TRUE(IsPayButtonEnabled());

  // The accept button should not receive default focus.
  EXPECT_FALSE(GetByDialogViewID(DialogViewID::PAY_BUTTON)->HasFocus());
}

// The Enter key should not be accelerated for the main payment sheet; see
// https://crbug.com/40062377
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerTest, EnterDoesNotContinue) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  EXPECT_TRUE(IsViewVisible(DialogViewID::PAY_BUTTON));
  EXPECT_TRUE(IsViewVisible(DialogViewID::CANCEL_BUTTON));
  EXPECT_TRUE(IsPayButtonEnabled());

  // Trigger the 'Enter' accelerator - this should NOT be present and the
  // dispatch should fail.
  views::View* summary_sheet =
      GetByDialogViewID(DialogViewID::PAYMENT_REQUEST_SHEET);
  EXPECT_FALSE(summary_sheet->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
}

// Test that the content view of the payment sheet view is contained by a
// ScrollView.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerTest, ContentViewScrollable) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  views::View* sheet_view =
      GetByDialogViewID(DialogViewID::PAYMENT_REQUEST_SHEET);
  ASSERT_NE(nullptr, sheet_view);

  // The scroll view should be contained by the root sheet view.
  views::ScrollView* scroll_view =
      static_cast<views::ScrollView*>(GetChildByDialogViewID(
          sheet_view, DialogViewID::PAYMENT_SHEET_SCROLL_VIEW));
  ASSERT_NE(nullptr, scroll_view);
  ASSERT_NE(nullptr, scroll_view->contents());

  // The content view should be contained by the scroll view.
  EXPECT_NE(nullptr, GetChildByDialogViewID(scroll_view->contents(),
                                            DialogViewID::CONTENT_VIEW));
}

using PaymentSheetViewControllerNoShippingTest = PaymentRequestBrowserTestBase;

// If shipping and contact info are not requested, their rows should not be
// present.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       NoShippingNoContactRows) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  EXPECT_NE(nullptr,
            GetByDialogViewID(DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION));
  EXPECT_EQ(nullptr, GetByDialogViewID(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION));
  EXPECT_EQ(nullptr, GetByDialogViewID(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION));
  EXPECT_EQ(nullptr, GetByDialogViewID(
                         DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION));
}

}  // namespace payments
