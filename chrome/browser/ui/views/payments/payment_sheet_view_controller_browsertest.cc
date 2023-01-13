// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/mock_input_event_activation_protector.h"

namespace payments {

class PaymentSheetViewControllerTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentSheetViewControllerTest() = default;
  ~PaymentSheetViewControllerTest() override = default;

  void OnPayCalled() override { pay_was_called_ = true; }

 protected:
  bool pay_was_called_ = false;
};

// The [Continue] button should be protected against accidental double-inputs.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerTest,
                       ContinueButtonIgnoresAccidentalInputs) {
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

  // Set up a mock input protector, which ignores the first input and then
  // accepts all subsequent inputs.
  auto input_protector =
      std::make_unique<views::MockInputEventActivationProtector>();
  EXPECT_CALL(*input_protector, IsPossiblyUnintendedInteraction)
      .WillOnce(testing::Return(true))
      .WillRepeatedly(testing::Return(false));

  views::View* sheet_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_REQUEST_SHEET));
  static_cast<PaymentSheetViewController*>(
      dialog_view()->controller_map_for_testing()->at(sheet_view).get())
      ->SetInputEventActivationProtectorForTesting(std::move(input_protector));

  // Because of the input protector, the first press of the button should be
  // ignored.
  views::View* button_view =
      dialog_view()->GetViewByID(static_cast<int>(DialogViewID::PAY_BUTTON));
  ClickOnDialogView(button_view);
  EXPECT_FALSE(pay_was_called_);

  // However a subsequent press should result in Pay() being called.
  ClickOnDialogView(button_view);
  EXPECT_TRUE(pay_was_called_);
}

// The 'Continue' or 'Cancel' buttons should not be auto-focused; see
// https://crbug.com/1403539
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

  // Neither of the actionable buttons should receive default focus.
  EXPECT_FALSE(dialog_view()
                   ->GetViewByID(static_cast<int>(DialogViewID::PAY_BUTTON))
                   ->HasFocus());
  EXPECT_FALSE(dialog_view()
                   ->GetViewByID(static_cast<int>(DialogViewID::CANCEL_BUTTON))
                   ->HasFocus());
}

// The Enter key should not be accelerated for the main payment sheet; see
// https://crbug.com/1403539
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
  views::View* summary_sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_REQUEST_SHEET));
  EXPECT_FALSE(summary_sheet->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
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

  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION)));
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION)));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION)));
}

typedef PaymentRequestBrowserTestBase PaymentHandlerUITest;

IN_PROC_BROWSER_TEST_F(PaymentHandlerUITest, BackReturnsToPaymentSheet) {
  NavigateTo("/payment_handler.html");

  // Add an autofill profile so the [Continue] button is enabled.
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);

  // Installs a payment handler which opens a window.
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  WaitForObservedEvent();

  EXPECT_TRUE(IsPayButtonEnabled());
  EXPECT_FALSE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET));

  // Click on Pay to show the payment handler window. The presence of Pay
  // indicates that the payment sheet is presenting.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  EXPECT_TRUE(IsViewVisible(DialogViewID::BACK_BUTTON));
  EXPECT_TRUE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET));

  // Click on back arrow to return to the payment sheet.
  ClickOnBackArrow();

  EXPECT_TRUE(IsPayButtonEnabled());
  EXPECT_FALSE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET));
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerUITest, BackAbortsRequestIfSkipSheet) {
  NavigateTo("/payment_handler.html");
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  // Skip the sheet flow skips directly to the payment handler window.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED});

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       content::JsReplace(
                                           "launchWithoutWaitForResponse($1)",
                                           payment_method)));
  WaitForObservedEvent();

  EXPECT_TRUE(IsViewVisible(DialogViewID::BACK_BUTTON));
  EXPECT_TRUE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET));

  // Click on back arrow aborts the payment request.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::BACK_BUTTON,
                           /* wait_for_animation= */ false);
}

}  // namespace payments
