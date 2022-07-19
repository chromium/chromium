// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "content/public/test/browser_test.h"

namespace payments {

using PaymentRequestMissingFieldsMetricsTest = PaymentRequestBrowserTestBase;

// Tests that proper UMA metrics are logged when shipping section is incomplete.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestIncompleteShippingProfile) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_shipping_address_instance_test.html");
  base::HistogramTester histogram_tester;

  // Add an incomplete profile. The profile has email address only.
  AddAutofillProfile(autofill::test::GetIncompleteProfile2());

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  int32_t expected_event_bits =
      JourneyLogger::EVENT_SHOWN | JourneyLogger::EVENT_USER_ABORTED |
      JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT |
      JourneyLogger::EVENT_REQUEST_SHIPPING |
      JourneyLogger::EVENT_REQUEST_METHOD_OTHER |
      JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER |
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
}

// Tests that proper UMA metrics are logged when contacts section is incomplete.
IN_PROC_BROWSER_TEST_F(PaymentRequestMissingFieldsMetricsTest,
                       TestIncompleteContactDetails) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_contact_details_test.html");
  base::HistogramTester histogram_tester;

  // Add an incomplete profile. The profile has email address only.
  AddAutofillProfile(autofill::test::GetIncompleteProfile2());

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

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
      JourneyLogger::EVENT_REQUEST_METHOD_OTHER |
      JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER |
      JourneyLogger::EVENT_NEEDS_COMPLETION_CONTACT_INFO;
  histogram_tester.ExpectBucketCount("PaymentRequest.Events",
                                     expected_event_bits, 1);

  // Since the incomplete profile has email address only, the rest of the bits
  // should be logged in MissingContactFields.
  int32_t expected_missing_contact_bits =
      PaymentsProfileComparator::kName | PaymentsProfileComparator::kPhone;
  histogram_tester.ExpectBucketCount("PaymentRequest.MissingContactFields",
                                     expected_missing_contact_bits, 1);

  // Even though the profile is incomplete, there should be no log for missing
  // shipping fields since shipping was not required.
  histogram_tester.ExpectTotalCount("PaymentRequest.MissingShippingFields", 0);
}

}  // namespace payments
