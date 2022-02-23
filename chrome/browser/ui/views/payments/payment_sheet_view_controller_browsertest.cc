// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

// A simple PaymentRequest which simply requests 'visa' or 'mastercard' and
// nothing else.
class PaymentSheetViewControllerNoShippingTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentSheetViewControllerNoShippingTest(
      const PaymentSheetViewControllerNoShippingTest&) = delete;
  PaymentSheetViewControllerNoShippingTest& operator=(
      const PaymentSheetViewControllerNoShippingTest&) = delete;

 protected:
  PaymentSheetViewControllerNoShippingTest() {
    feature_list_.InitAndEnableFeature(features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// With no data present, the pay button should be disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest, NoData) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present, the pay button should be enabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       SupportedCard) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  EXPECT_TRUE(IsPayButtonEnabled());

  // When an autofill payment app is selected the primary button should have
  // "Pay" label.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_PAY_BUTTON),
            GetPrimaryButtonLabel());
}

// With only an unsupported card (Amex) in the database, the pay button should
// be disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       UnsupportedCard) {
  NavigateTo("/payment_request_no_shipping_test.html");
  AddCreditCard(autofill::test::GetCreditCard2());  // Amex card.

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// If shipping and contact info are not requested, their rows should not be
// present.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       NoShippingNoContactRows) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION)));
  EXPECT_NE(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON)));
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION)));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION)));
}

// The tests in this class correspond to the tests of the same name in
// PaymentSheetViewControllerNoShippingTest, with the basic-card being disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentSheetViewControllerNoShippingBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentSheetViewControllerNoShippingBasicCardDisabledTest(
      const PaymentSheetViewControllerNoShippingBasicCardDisabledTest&) =
      delete;
  PaymentSheetViewControllerNoShippingBasicCardDisabledTest& operator=(
      const PaymentSheetViewControllerNoShippingBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentSheetViewControllerNoShippingBasicCardDisabledTest() {
    feature_list_.InitWithFeatures({}, {::features::kPaymentRequestBasicCard});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If shipping and contact info are not requested, their rows should not be
// present.
IN_PROC_BROWSER_TEST_F(
    PaymentSheetViewControllerNoShippingBasicCardDisabledTest,
    NoShippingNoContactRows) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "install()"));

  ResetEventWaiterForDialogOpened();
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "paymentRequestWithOptions({requestShipping: true})"));
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
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "install()"));

  // Skip the sheet flow skips directly to the payment handler window.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED});

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       "launchWithoutWaitForResponse()"));
  WaitForObservedEvent();

  EXPECT_TRUE(IsViewVisible(DialogViewID::BACK_BUTTON));
  EXPECT_TRUE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET));

  // Click on back arrow aborts the payment request.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::BACK_BUTTON,
                           /* wait_for_animation= */ false);
}

}  // namespace payments
