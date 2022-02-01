// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace payments {

class PaymentRequestRetryTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRetryTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRetryTest,
                       DoNotAllowPaymentInstrumentChange) {
  NavigateTo("/payment_request_retry_with_payer_errors.html");
  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  AddAutofillProfile(contact);

  // Add two credit cards and record usage for the Visa card to ensure that it
  // gets preselected.
  autofill::CreditCard visa_card = autofill::test::GetCreditCard();
  visa_card.set_billing_address_id(contact.guid());
  visa_card.RecordAndLogUse();
  AddCreditCard(visa_card);
  autofill::CreditCard amex_card = autofill::test::GetCreditCard2();
  amex_card.set_billing_address_id(contact.guid());
  AddCreditCard(amex_card);

  // Confirm that there are two payment apps available.
  InvokePaymentRequestUI();
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(2U, request->state()->available_apps().size());

  // Enter a valid CVC format for the Visa card.
  PayWithCreditCard(u"123");

  // Confirm that only one payment app is available for retry().
  RetryPaymentRequest(
      "{"
      "  payer: {"
      "    email: 'EMAIL ERROR',"
      "    name: 'NAME ERROR',"
      "    phone: 'PHONE ERROR'"
      "  }"
      "}",
      DialogEvent::CONTACT_INFO_EDITOR_OPENED, dialog_view());
  EXPECT_EQ(1U, request->state()->available_apps().size());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestRetryTest, DisableAddCardDuringRetry) {
  NavigateTo("/payment_request_retry_with_payer_errors.html");
  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  AddAutofillProfile(contact);

  autofill::CreditCard visa_card = autofill::test::GetCreditCard();
  visa_card.set_billing_address_id(contact.guid());
  AddCreditCard(visa_card);

  // Confirm that "Add card" button is enabled in payment list view.
  InvokePaymentRequestUI();
  OpenPaymentMethodScreen();
  views::View* add_card_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON));
  EXPECT_TRUE(add_card_button);
  EXPECT_TRUE(add_card_button->GetEnabled());
  ClickOnBackArrow();

  // Enter a valid CVC format for the Visa card.
  PayWithCreditCard(u"123");

  // Confirm that "Add card" button does not exist in payment list view.
  RetryPaymentRequest(
      "{"
      "  payer: {"
      "    email: 'EMAIL ERROR',"
      "    name: 'NAME ERROR',"
      "    phone: 'PHONE ERROR'"
      "  }"
      "}",
      DialogEvent::CONTACT_INFO_EDITOR_OPENED, dialog_view());
  OpenPaymentMethodScreen();
  add_card_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON));
  EXPECT_EQ(nullptr, add_card_button);
}

// The tests in this class correspond to the tests of the same name in
// PaymentRequestRetryTest, with basic-card disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentRequestRetryBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRetryBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRetryBasicCardDisabledTest,
                       DoNotAllowPaymentInstrumentChange) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_retry_with_payer_errors.html");
  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  AddAutofillProfile(contact);

  // Confirm that there are two payment apps available.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(2U, request->state()->available_apps().size());

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Wait for the response to settle.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "processShowResponse();"));

  // Confirm that only one payment app is available for retry().
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::CONTACT_INFO_EDITOR_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "retry({"
                                     "  payer: {"
                                     "    email: 'EMAIL ERROR',"
                                     "    name: 'NAME ERROR',"
                                     "    phone: 'PHONE ERROR'"
                                     "  }"
                                     "});"));
  WaitForObservedEvent();
  EXPECT_EQ(1U, request->state()->available_apps().size());
}

}  // namespace payments
