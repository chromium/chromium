// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
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

namespace payments {

using PaymentSheetViewControllerNoShippingTest = PaymentRequestBrowserTestBase;

// If shipping and contact info are not requested, their rows should not be
// present.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       NoShippingNoContactRows) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");

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

  // Add an autofill profile and credit card so the payment sheet is shown.
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  // Installs a payment handler which opens a window.
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  ResetEventWaiterForDialogOpened();
  EXPECT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "paymentRequestWithOptions({requestShipping: true}, $1)",
                    payment_method)));
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
