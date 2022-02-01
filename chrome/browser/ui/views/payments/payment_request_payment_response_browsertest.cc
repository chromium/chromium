// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestPaymentResponseAutofillPaymentAppTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestPaymentResponseAutofillPaymentAppTest(
      const PaymentRequestPaymentResponseAutofillPaymentAppTest&) = delete;
  PaymentRequestPaymentResponseAutofillPaymentAppTest& operator=(
      const PaymentRequestPaymentResponseAutofillPaymentAppTest&) = delete;

 protected:
  PaymentRequestPaymentResponseAutofillPaymentAppTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the PaymentResponse contains all the required fields for an
// Autofill payment app.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentResponseAutofillPaymentAppTest,
                       TestPaymentResponse) {
  NavigateTo("/payment_request_no_shipping_test.html");
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(u"123");

  // Test that the card details were sent to the merchant.
  ExpectBodyContains(
      {"\"cardNumber\": \"4111111111111111\"", "\"cardSecurityCode\": \"123\"",
       "\"cardholderName\": \"Test User\"",
       base::StringPrintf(
           "\"expiryMonth\": \"%s\"",
           base::UTF16ToUTF8(card.Expiration2DigitMonthAsString()).c_str()),
       base::StringPrintf(
           "\"expiryYear\": \"%s\"",
           base::UTF16ToUTF8(card.Expiration4DigitYearAsString()).c_str())});

  // Test that the billing address was sent to the merchant.
  ExpectBodyContains(
      {"\"billingAddress\": {", "\"666 Erebus St.\"", "\"Apt 8\"",
       "\"city\": \"Elysium\"", "\"dependentLocality\": \"\"",
       "\"country\": \"US\"", "\"sortingCode\": \"\"",
       "\"organization\": \"Underworld\"", "\"phone\": \"+16502111111\"",
       "\"postalCode\": \"91111\"", "\"recipient\": \"John H. Doe\"",
       "\"region\": \"CA\""});
}

class PaymentRequestPaymentResponseShippingAddressTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestPaymentResponseShippingAddressTest(
      const PaymentRequestPaymentResponseShippingAddressTest&) = delete;
  PaymentRequestPaymentResponseShippingAddressTest& operator=(
      const PaymentRequestPaymentResponseShippingAddressTest&) = delete;

 protected:
  PaymentRequestPaymentResponseShippingAddressTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the PaymentResponse contains all the required fields for a
// shipping address and shipping option.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentResponseShippingAddressTest,
                       TestPaymentResponse) {
  NavigateTo("/payment_request_free_shipping_test.html");
  // Create a billing address and a card that uses it.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Create a shipping address with a higher frecency score, so that it is
  // selected as the defautl shipping address.
  autofill::AutofillProfile shipping_address =
      autofill::test::GetFullProfile2();
  shipping_address.set_use_count(2000);
  AddAutofillProfile(shipping_address);

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(u"123");

  // Test that the shipping address was sent to the merchant.
  ExpectBodyContains(
      {"\"country\": \"US\"", "\"123 Main Street\"", "\"Unit 1\"",
       "\"region\": \"MI\"", "\"city\": \"Greensdale\"",
       "\"dependentLocality\": \"\"", "\"postalCode\": \"48838\"",
       "\"sortingCode\": \"\"", "\"organization\": \"ACME\"",
       "\"recipient\": \"Jane A. Smith\"", "\"phone\": \"+13105557889\""});

  // Test that the shipping option was sent to the merchant.
  ExpectBodyContains({"\"shippingOption\": \"freeShippingOption\""});
}

// The tests in this class correspond to the tests of the same name in
// PaymentRequestPaymentResponseShippingAddressTest, but with basic-card
// disabled. Parameterized tests are not used because the test setup for both
// tests are too different.
class PaymentRequestPaymentResponseShippingAddressBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestPaymentResponseShippingAddressBasicCardDisabledTest(
      const PaymentRequestPaymentResponseShippingAddressBasicCardDisabledTest&) =
      delete;
  PaymentRequestPaymentResponseShippingAddressBasicCardDisabledTest& operator=(
      const PaymentRequestPaymentResponseShippingAddressBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentRequestPaymentResponseShippingAddressBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the PaymentResponse contains all the required fields for a
// shipping address and shipping option.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentResponseShippingAddressBasicCardDisabledTest,
    TestPaymentResponse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_free_shipping_test.html");

  // Create two shipping addresses, one with a higher frequency score so that it
  // is selected as the default shipping address.
  autofill::AutofillProfile shipping_address1 = autofill::test::GetFullProfile();
  AddAutofillProfile(shipping_address1);
  autofill::AutofillProfile shipping_address2 =
      autofill::test::GetFullProfile2();
  shipping_address2.set_use_count(2000);
  AddAutofillProfile(shipping_address2);

  // Complete the Payment Request.
  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  WaitForObservedEvent();

  // Test that the shipping address was sent to the merchant.
  ExpectBodyContains(
      {"\"country\": \"US\"", "\"123 Main Street\"", "\"Unit 1\"",
       "\"region\": \"MI\"", "\"city\": \"Greensdale\"",
       "\"dependentLocality\": \"\"", "\"postalCode\": \"48838\"",
       "\"sortingCode\": \"\"", "\"organization\": \"ACME\"",
       "\"recipient\": \"Jane A. Smith\"", "\"phone\": \"+13105557889\""});

  // Test that the shipping option was sent to the merchant.
  ExpectBodyContains({"\"shippingOption\": \"freeShippingOption\""});
}

class PaymentRequestPaymentResponseAllContactDetailsTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestPaymentResponseAllContactDetailsTest(
      const PaymentRequestPaymentResponseAllContactDetailsTest&) = delete;
  PaymentRequestPaymentResponseAllContactDetailsTest& operator=(
      const PaymentRequestPaymentResponseAllContactDetailsTest&) = delete;

 protected:
  PaymentRequestPaymentResponseAllContactDetailsTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the PaymentResponse contains all the required fields for contact
// details when all three details are requested.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentResponseAllContactDetailsTest,
                       TestPaymentResponse) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(u"123");

  // Test that the contact details were sent to the merchant.
  ExpectBodyContains({"\"payerName\": \"John H. Doe\"",
                      "\"payerEmail\": \"johndoe@hades.com\"",
                      "\"payerPhone\": \"+16502111111\""});
}

// Tests that the PaymentResponse contains all the correct contact details
// when user changes the selected contact information after retry() called once.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentResponseAllContactDetailsTest,
    RetryWithPayerErrors_HasSameValueButDifferentErrorsShown) {
  NavigateTo("/payment_request_retry_with_payer_errors.html");

  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  contact.set_use_count(1000);
  AddAutofillProfile(contact);

  autofill::AutofillProfile contact2 = autofill::test::GetFullProfile2();
  AddAutofillProfile(contact2);

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(contact.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  PayWithCreditCard(u"123");
  ExpectBodyContains({"\"payerName\": \"John H. Doe\"",
                      "\"payerEmail\": \"johndoe@hades.com\"",
                      "\"payerPhone\": \"+16502111111\""});

  RetryPaymentRequest("{}", dialog_view());

  // Select "contact2" profile
  OpenContactInfoScreen();
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW));
  DCHECK(list_view);
  ClickOnDialogViewAndWait(list_view->children()[1]);

  ExpectBodyContains({"\"payerName\": \"Jane A. Smith\"",
                      "\"payerEmail\": \"jsmith@example.com\"",
                      "\"payerPhone\": \"+13105557889\""});
}

class PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest(
      const PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest&) =
      delete;
  PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest&
  operator=(
      const PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the PaymentResponse contains all the required fields for contact
// details when all three details are requested.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest,
    TestPaymentResponse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);

  // Complete the Payment Request.
  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  WaitForObservedEvent();

  // Test that the contact details were sent to the merchant.
  ExpectBodyContains({"\"payerName\": \"John H. Doe\"",
                      "\"payerEmail\": \"johndoe@hades.com\"",
                      "\"payerPhone\": \"+16502111111\""});
}

// Tests that the PaymentResponse contains all the correct contact details
// when user changes the selected contact information after retry() called once.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentResponseAllContactDetailsBasicCardDisabledTest,
    RetryWithPayerErrors_HasSameValueButDifferentErrorsShown) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_retry_with_payer_errors.html");

  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  contact.set_use_count(1000);
  AddAutofillProfile(contact);

  autofill::AutofillProfile contact2 = autofill::test::GetFullProfile2();
  AddAutofillProfile(contact2);

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "processShowResponse();"));
  ExpectBodyContains({"\"payerName\": \"John H. Doe\"",
                      "\"payerEmail\": \"johndoe@hades.com\"",
                      "\"payerPhone\": \"+16502111111\""});

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "retry({});"));

  // Select "contact2" profile
  OpenContactInfoScreen();
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW));
  DCHECK(list_view);
  ClickOnDialogViewAndWait(list_view->children()[1]);

  ExpectBodyContains({"\"payerName\": \"Jane A. Smith\"",
                      "\"payerEmail\": \"jsmith@example.com\"",
                      "\"payerPhone\": \"+13105557889\""});
}

class PaymentRequestPaymentResponseOneContactDetailTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestPaymentResponseOneContactDetailTest(
      const PaymentRequestPaymentResponseOneContactDetailTest&) = delete;
  PaymentRequestPaymentResponseOneContactDetailTest& operator=(
      const PaymentRequestPaymentResponseOneContactDetailTest&) = delete;

 protected:
  PaymentRequestPaymentResponseOneContactDetailTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the PaymentResponse contains all the required fields for contact
// details when all ont detail is requested.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentResponseOneContactDetailTest,
                       TestPaymentResponse) {
  NavigateTo("/payment_request_email_and_free_shipping_test.html");
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(u"123");

  // Test that the contact details were sent to the merchant.
  ExpectBodyContains({"\"payerName\": null",
                      "\"payerEmail\": \"johndoe@hades.com\"",
                      "\"payerPhone\": null"});
}

class PaymentRequestPaymentResponseOneContactDetailBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestPaymentResponseOneContactDetailBasicCardDisabledTest(
      const PaymentRequestPaymentResponseOneContactDetailBasicCardDisabledTest&) =
      delete;
  PaymentRequestPaymentResponseOneContactDetailBasicCardDisabledTest& operator=(
      const PaymentRequestPaymentResponseOneContactDetailBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentRequestPaymentResponseOneContactDetailBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the PaymentResponse contains all the required fields for contact
// details when all ont detail is requested.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentResponseOneContactDetailBasicCardDisabledTest,
    TestPaymentResponse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_email_and_free_shipping_test.html");
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);

  // Complete the Payment Request.
  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  WaitForObservedEvent();

  // Test that the contact details were sent to the merchant.
  ExpectBodyContains({"\"payerName\": null",
                      "\"payerEmail\": \"johndoe@hades.com\"",
                      "\"payerPhone\": null"});
}

}  // namespace payments
