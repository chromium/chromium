// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// A simple PaymentRequest which simply requests 'visa' or 'mastercard' and
// nothing else.
class PaymentSheetViewControllerNoShippingTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentSheetViewControllerNoShippingTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentSheetViewControllerNoShippingTest);
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

// Accepts 'visa' cards and requests the full contact details.
class PaymentSheetViewControllerContactDetailsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentSheetViewControllerContactDetailsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentSheetViewControllerContactDetailsTest);
};

// With no data present, the pay button should be disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest, NoData) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  InvokePaymentRequestUI();

  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present, the pay button is still disabled
// because there is no contact details.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       SupportedCard_NoContactInfo) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  AddCreditCard(autofill::test::GetCreditCard());  // Visa card.

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present and a complete address profile, there is
// enough information to enable the pay button.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       SupportedCard_CompleteContactInfo) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  EXPECT_TRUE(IsPayButtonEnabled());
}

// With only an unsupported card present and a complete address profile, the pay
// button is disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       UnsupportedCard_CompleteContactInfo) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  AddCreditCard(autofill::test::GetCreditCard2());  // Amex card.
  AddAutofillProfile(autofill::test::GetFullProfile());

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present and a *incomplete* address profile, the
// pay button is disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       SupportedCard_IncompleteContactInfo) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  AddCreditCard(autofill::test::GetCreditCard());  // Visa card.

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  // Remove the name from the profile to be stored.
  profile.SetRawInfo(autofill::NAME_FIRST, base::ASCIIToUTF16(""));
  profile.SetRawInfo(autofill::NAME_MIDDLE, base::ASCIIToUTF16(""));
  profile.SetRawInfo(autofill::NAME_LAST, base::ASCIIToUTF16(""));
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// Payment sheet view skips showing shipping section when the selected
// instrument supports shipping delegation, the pay button is enabled with blank
// autofill data.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       ShippingDelegation) {
  // Install a payment handler which supports shipping delegation.
  NavigateTo("/payment_handler.html");
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['shippingAddress'])"));
  // Invoke a payment request with basic-card and methodName =
  // window.location.origin + '/pay' supportedMethods (see payment_handler.js).
  ResetEventWaiterForDialogOpened();
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "paymentRequestWithOptions({requestShipping: true})"));
  WaitForObservedEvent();

  // Verify that no autofill profile exists.
  EXPECT_TRUE(GetDataManager()->GetProfiles().empty());

  // Shipping address and shipping option sections are not shown in the payment
  // sheet view since handling shipping address is delegated to the selected
  // payment handler (payment_handler.js).
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON)));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));

  // Payment button should be enabled with blank autofill profiles since the
  // payment handler supports shipping delegation.
  EXPECT_TRUE(IsPayButtonEnabled());
}

// Payment sheet view skips showing contact section when the selected instrument
// supports contact delegation.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       ContactDelegation) {
  // Install a payment handler which supports contact delegation.
  NavigateTo("/payment_handler.html");
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ(
      expected,
      content::EvalJs(
          GetActiveWebContents(),
          "enableDelegations(['payerName', 'payerPhone', 'payerEmail'])"));
  // Invoke a payment request with basic-card and methodName =
  // window.location.origin + '/pay' supportedMethods (see payment_handler.js).
  ResetEventWaiterForDialogOpened();
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "paymentRequestWithOptions({requestPayerName: true, "
                      "requestPayerPhone: true, requestPayerEmail: true})"));
  WaitForObservedEvent();

  // Verify that no autofill profile exists.
  EXPECT_TRUE(GetDataManager()->GetProfiles().empty());

  // Contact info section is not shown in the payment sheet view since handling
  // required contact information is delegated to the selected payment handler
  // (payment_handler.js).
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON)));

  // Payment button should be enabled with blank autofill profiles since the
  // payment handler supports contact delegation.
  EXPECT_TRUE(IsPayButtonEnabled());
}

// Payment sheet view shows shipping section when the selected instrument
// supports contact delegation only.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       ContactOnlyDelegationShippingRequested) {
  // Install a payment handler which supports contact delegation.
  NavigateTo("/payment_handler.html");
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ(
      expected,
      content::EvalJs(
          GetActiveWebContents(),
          "enableDelegations(['payerName', 'payerPhone', 'payerEmail'])"));
  // Invoke a payment request with basic-card and methodName =
  // window.location.origin + '/pay' supportedMethods (see payment_handler.js).
  ResetEventWaiterForDialogOpened();
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "paymentRequestWithOptions({requestShipping: true})"));
  WaitForObservedEvent();

  // Verify that no autofill profile exists.
  EXPECT_TRUE(GetDataManager()->GetProfiles().empty());

  // Shipping section is still shown since the selected payment instrument does
  // not support delegation of shipping address.
  EXPECT_NE(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON)));

  // Payment button should be disabled since the browser should collect shipping
  // address.
  EXPECT_FALSE(IsPayButtonEnabled());
}

// Payment sheet view shows contact section when the selected instrument does
// not support delegation of all required contact details.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       PartialContactDelegation) {
  // Install a payment handler which supports delegation of all required contact
  // information except payer's email.
  NavigateTo("/payment_handler.html");
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['payerName', 'payerPhone'])"));
  // Invoke a payment request with basic-card and methodName =
  // window.location.origin + '/pay' supportedMethods (see payment_handler.js).
  ResetEventWaiterForDialogOpened();
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "paymentRequestWithOptions({requestPayerName: true, "
                      "requestPayerPhone: true, requestPayerEmail: true})"));
  WaitForObservedEvent();

  // Verify that no autofill profile exists.
  EXPECT_TRUE(GetDataManager()->GetProfiles().empty());

  // Contact info section is still shown since the selected payment instrument
  // does not support delegation of all required contact info.
  EXPECT_NE(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON)));

  // Payment button should be disabled since the browser should collect payer's
  // email.
  EXPECT_FALSE(IsPayButtonEnabled());
}

// If shipping and contact info are requested, show all the rows.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       AllRowsPresent) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  InvokePaymentRequestUI();

  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION)));
  // The buttons to select payment methods and shipping address are present.
  EXPECT_NE(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON)));
  EXPECT_NE(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON)));
  // Shipping option section (or its button) is not yet present.
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON)));
  // Contact details button is present.
  EXPECT_NE(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON)));
}

IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       AllClickableRowsPresent) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);
  InvokePaymentRequestUI();

  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION)));
  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION)));
  EXPECT_NE(nullptr,
            dialog_view()->GetViewByID(static_cast<int>(
                DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION)));
  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));
  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION)));
}

IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       RetryWithEmptyError) {
  NavigateTo("/payment_request_retry.html");

  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(address.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  PayWithCreditCard(base::ASCIIToUTF16("123"));
  RetryPaymentRequest("{}", dialog_view());

  EXPECT_EQ(base::ASCIIToUTF16(
                "There was an error processing your order. Please try again."),
            GetLabelText(DialogViewID::WARNING_LABEL));
}

IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       RetryWithError) {
  NavigateTo("/payment_request_retry.html");

  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(address.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  PayWithCreditCard(base::ASCIIToUTF16("123"));
  RetryPaymentRequest("{ error: 'ERROR MESSAGE' }", dialog_view());

  EXPECT_EQ(base::ASCIIToUTF16("ERROR MESSAGE"),
            GetLabelText(DialogViewID::WARNING_LABEL));
}

}  // namespace payments
