// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/core/autofill_card_validation.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payments_profile_comparator.h"

namespace payments {

using PaymentRequestMissingFieldsMetricsTest = PaymentRequestBrowserTestBase;

// Tests that proper UMA metrics are logged when payment section has no
// suggestion.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestMissingPaymentMethod) {
  NavigateTo("/payment_request_shipping_address_instance_test.html");
  base::HistogramTester histogram_tester;

  // Add an autofill profile for billing address without adding any cards.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);

  // Show a Payment Request.
  InvokePaymentRequestUI();

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  int32_t expected_event_bits = JourneyLogger::EVENT_SHOWN |
                                JourneyLogger::EVENT_USER_ABORTED |
                                JourneyLogger::EVENT_REQUEST_SHIPPING |
                                JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD |
                                JourneyLogger::EVENT_NEEDS_COMPLETION_PAYMENT;
  histogram_tester.ExpectBucketCount("PaymentRequest.Events",
                                     expected_event_bits, 1);

  // Since no card is added to the profile, all payment fields should be
  // missing.
  int32_t expected_missing_payment_bits =
      CREDIT_CARD_EXPIRED | CREDIT_CARD_NO_CARDHOLDER | CREDIT_CARD_NO_NUMBER |
      CREDIT_CARD_NO_BILLING_ADDRESS;
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingPaymentFields",
                                     expected_missing_payment_bits, 1);

  // There should be no log for missing shipping address fields since the
  // section had one complete suggestion.
  histogram_tester.ExpectTotalCount("PaymentRequest.MissingShippingFields", 0);
}

// Tests that proper UMA metrics are logged when payment section has incomplete
// card.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestIncompleteCard) {
  NavigateTo("/payment_request_shipping_address_instance_test.html");
  base::HistogramTester histogram_tester;

  // Add an autofill profile for billing address with a credit card missing card
  // holder's name.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetIncompleteCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa

  // Show a Payment Request.
  InvokePaymentRequestUI();

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged. Even though the suggested card
  // is incomplete, EVENT_HAD_INITIAL_FORM_OF_PAYMENT is set since it only
  // shows whether the payment suggestion list is empty or not.
  int32_t expected_event_bits =
      JourneyLogger::EVENT_SHOWN | JourneyLogger::EVENT_USER_ABORTED |
      JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT |
      JourneyLogger::EVENT_REQUEST_SHIPPING |
      JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_NEEDS_COMPLETION_PAYMENT;
  histogram_tester.ExpectBucketCount("PaymentRequest.Events",
                                     expected_event_bits, 1);

  histogram_tester.ExpectBucketCount("PaymentRequest.MissingPaymentFields",
                                     CREDIT_CARD_NO_CARDHOLDER, 1);

  // There should be no log for missing shipping address fields since the
  // section had one complete suggestion.
  histogram_tester.ExpectTotalCount("PaymentRequest.MissingShippingFields", 0);
}

// Tests that proper UMA metrics are logged when payment section has expired
// card.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestExpiredCard) {
  NavigateTo("/payment_request_shipping_address_instance_test.html");
  base::HistogramTester histogram_tester;

  // Add an autofill profile for billing address with an expired card.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetExpiredCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa

  // Show a Payment Request.
  InvokePaymentRequestUI();

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  // EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS is set since expired cards are
  // treated as complete.
  int32_t expected_event_bits =
      JourneyLogger::EVENT_SHOWN | JourneyLogger::EVENT_USER_ABORTED |
      JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT |
      JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS |
      JourneyLogger::EVENT_REQUEST_SHIPPING |
      JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD;
  histogram_tester.ExpectBucketCount("PaymentRequest.Events",
                                     expected_event_bits, 1);

  // Even though expired cards are treated as complete, we still record the
  // MissingPaymentFields with expired bit set.
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingPaymentFields",
                                     CREDIT_CARD_EXPIRED, 1);

  // There should be no log for missing shipping address fields since the
  // section had one complete suggestion.
  histogram_tester.ExpectTotalCount("PaymentRequest.MissingShippingFields", 0);
}

// Tests that proper UMA metrics are logged when shipping section is incomplete.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestIncompleteShippingProfile) {
  NavigateTo("/payment_request_shipping_address_instance_test.html");
  base::HistogramTester histogram_tester;

  // Add an incomplete profile for billing address. The profile has email
  // address only.
  autofill::AutofillProfile billing_address =
      autofill::test::GetIncompleteProfile2();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa

  // Show a Payment Request.
  InvokePaymentRequestUI();

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged. EVENT_NEEDS_COMPLETION_PAYMENT is
  // set since billing address of the card is incomplete.
  int32_t expected_event_bits =
      JourneyLogger::EVENT_SHOWN | JourneyLogger::EVENT_USER_ABORTED |
      JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT |
      JourneyLogger::EVENT_REQUEST_SHIPPING |
      JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_NEEDS_COMPLETION_PAYMENT |
      JourneyLogger::EVENT_NEEDS_COMPLETION_SHIPPING;
  histogram_tester.ExpectBucketCount("PaymentRequest.Events",
                                     expected_event_bits, 1);

  // Since the incomplete profile has email address only, the rest of the bits
  // should be logged in MissingShippingFields.
  int32_t expected_missing_shipping_bits = PaymentsProfileComparator::kName |
                                           PaymentsProfileComparator::kPhone |
                                           PaymentsProfileComparator::kAddress;
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingShippingFields",
                                     expected_missing_shipping_bits, 1);

  // Ensure that the billing address of the card is incomplete.
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingPaymentFields",
                                     CREDIT_CARD_NO_BILLING_ADDRESS, 1);
}

// Tests that proper UMA metrics are logged when contacts section is incomplete.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestIncompleteContactDetails) {
  NavigateTo("/payment_request_contact_details_test.html");
  base::HistogramTester histogram_tester;

  // Add an incomplete profile for billing address. The profile has email
  // address only.
  autofill::AutofillProfile billing_address =
      autofill::test::GetIncompleteProfile2();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa

  // Show a Payment Request.
  InvokePaymentRequestUI();

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged. EVENT_NEEDS_COMPLETION_PAYMENT is
  // set since billing address of the card is incomplete.
  int32_t expected_event_bits =
      JourneyLogger::EVENT_SHOWN | JourneyLogger::EVENT_USER_ABORTED |
      JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT |
      JourneyLogger::EVENT_REQUEST_PAYER_NAME |
      JourneyLogger::EVENT_REQUEST_PAYER_EMAIL |
      JourneyLogger::EVENT_REQUEST_PAYER_PHONE |
      JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_REQUEST_METHOD_OTHER |
      JourneyLogger::EVENT_NEEDS_COMPLETION_CONTACT_INFO |
      JourneyLogger::EVENT_NEEDS_COMPLETION_PAYMENT;
  histogram_tester.ExpectBucketCount("PaymentRequest.Events",
                                     expected_event_bits, 1);

  // Since the incomplete profile has email address only, the rest of the bits
  // should be logged in MissingContactFields.
  int32_t expected_missing_contact_bits =
      PaymentsProfileComparator::kName | PaymentsProfileComparator::kPhone;
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingContactFields",
                                     expected_missing_contact_bits, 1);

  // Ensure that the billing address of the card is incomplete.
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingPaymentFields",
                                     CREDIT_CARD_NO_BILLING_ADDRESS, 1);

  // Even though the profile is incomplete, there should be no log for missing
  // shipping fields since shipping was not required.
  histogram_tester.ExpectTotalCount("PaymentRequest.MissingShippingFields", 0);
}

// Tests that proper UMA metrics are logged when the available card type does
// not exactly match the specified type in payment request.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestCardWithMismatchedType) {
  NavigateTo("/payment_request_debit_test.html");
  base::HistogramTester histogram_tester;
  // Add a profile for billing address with a visa card type.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard visa_card = autofill::test::GetCreditCard2();
  visa_card.set_billing_address_id(billing_address.guid());
  AddCreditCard(visa_card);

  // Show a Payment Request with debit card specified.
  InvokePaymentRequestUI();

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged. EVENT_NEEDS_COMPLETION_PAYMENT is
  // set since the type of the saved card does not match the specified type in
  // payment request.
  int32_t expected_event_bits =
      JourneyLogger::EVENT_SHOWN | JourneyLogger::EVENT_USER_ABORTED |
      JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT |
      JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD |
      JourneyLogger::EVENT_NEEDS_COMPLETION_PAYMENT;
  histogram_tester.ExpectBucketCount("PaymentRequest.Events",
                                     expected_event_bits, 1);

  // Ensure that the card type does not exactly match the payment request
  // network type.
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingPaymentFields",
                                     CREDIT_CARD_TYPE_MISMATCH, 1);
}

}  // namespace payments
