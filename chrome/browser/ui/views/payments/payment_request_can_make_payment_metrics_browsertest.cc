// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace payments {

class PaymentRequestCanMakePaymentMetricsTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestCanMakePaymentMetricsTest(
      const PaymentRequestCanMakePaymentMetricsTest&) = delete;
  PaymentRequestCanMakePaymentMetricsTest& operator=(
      const PaymentRequestCanMakePaymentMetricsTest&) = delete;

 protected:
  PaymentRequestCanMakePaymentMetricsTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

  void SetupInitialAddressAndCreditCard() {
    autofill::AutofillProfile billing_address =
        autofill::test::GetFullProfile();
    AddAutofillProfile(billing_address);
    autofill::CreditCard card = autofill::test::GetCreditCard();
    card.set_billing_address_id(billing_address.guid());
    AddCreditCard(card);
  }

  void CheckPaymentSupportAndThenShow() {
    // Start the Payment Request and expect CanMakePayment and
    // HasEnrolledInstrument to be called before the Payment Request is shown.
    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                                 DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                                 DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED,
                                 DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::DIALOG_OPENED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
    WaitForObservedEvent();
    // Wait for all callbacks to run.
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_Completed) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so
  // HasEnrolledInstrument returns true.
  SetupInitialAddressAndCreditCard();

  CheckPaymentSupportAndThenShow();

  // Complete the Payment Request.
  PayWithCreditCardAndWait(u"123");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_OtherAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so
  // HasEnrolledInstrument returns true.
  SetupInitialAddressAndCreditCard();

  CheckPaymentSupportAndThenShow();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_UserAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so
  // HasEnrolledInstrument returns true.
  SetupInitialAddressAndCreditCard();

  CheckPaymentSupportAndThenShow();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_Completed) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // An address is needed so that the UI can choose it as a billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);

  // Don't add a card on file, so HasEnrolledInstrument returns false.
  CheckPaymentSupportAndThenShow();

  // Add a test credit card.
  OpenCreditCardEditorScreen();
  SetEditorTextfieldValue(u"Bob Simpson", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u"4111111111111111", autofill::CREDIT_CARD_NUMBER);
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  SelectBillingAddress(billing_address.guid());
  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  // Complete the Payment Request.
  PayWithCreditCardAndWait(u"123");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_OtherAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Don't add a card on file, so HasEnrolledInstrument returns false.
  CheckPaymentSupportAndThenShow();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_UserAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Don't add a card on file, so HasEnrolledInstrument returns false.
  CheckPaymentSupportAndThenShow();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_NotShown) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so
  // HasEnrolledInstrument returns true.
  SetupInitialAddressAndCreditCard();

  // Try to start the Payment Request, but only check payment support without
  // calling show().
  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryNoShow();"));
  WaitForObservedEvent();

  // Navigate away to trigger the log.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_NotShown) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Don't add a card on file, so HasEnrolledInstrument returns false.
  // Try to start the Payment Request, but only check payment support without
  // calling show().
  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryNoShow();"));
  WaitForObservedEvent();

  // Navigate away to trigger the log.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_Completed) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address to make it simpler
  // to complete the Payment Request.
  SetupInitialAddressAndCreditCard();

  ResetEventWaiterForDialogOpened();
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Complete the Payment Request.
  PayWithCreditCardAndWait(u"123");

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_OtherAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address to make it simpler
  // to complete the Payment Request.
  SetupInitialAddressAndCreditCard();

  ResetEventWaiterForDialogOpened();
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_UserAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address to make it simpler
  // to complete the Payment Request.
  SetupInitialAddressAndCreditCard();

  ResetEventWaiterForDialogOpened();
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_NavigationToSameOrigin) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  CheckPaymentSupportAndThenShow();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on the same origin.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  NavigateTo("/payment_request_email_test.html");
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_NavigationToDifferentOrigin) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  CheckPaymentSupportAndThenShow();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on a different origin.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  GURL other_origin_url =
      https_server()->GetURL("b.com", "/payment_request_email_test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_origin_url));
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_TabClose) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  CheckPaymentSupportAndThenShow();

  // Simulate that the user closes the tab containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::CloseTab(browser());
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_Reload) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  CheckPaymentSupportAndThenShow();

  // Simulate that the user reloads the page containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

// The tests in this class correspond to the tests of the same name in
// PaymentRequestCanMakePaymentMetricsTest, with the basic-card being disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest
    : public PaymentRequestCanMakePaymentMetricsTest {
 public:
  PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest(
      const PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest&) =
      delete;
  PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest& operator=(
      const PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest&) =
      delete;
  net::EmbeddedTestServer nickpay_server_;

 protected:
  PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest()
      : nickpay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures({}, {::features::kPaymentRequestBasicCard});
  }

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

    // Choosing nickpay for its JIT installation support.
    nickpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/nickpay.com/");

    ASSERT_TRUE(nickpay_server_.Start());
  }

  void InstallTwoPaymentHandlersAndQueryShow() {
    std::string a_method_name;
    InstallPaymentApp("a.com", "payment_request_success_responder.js",
                      &a_method_name);

    std::string b_method_name;
    InstallPaymentApp("b.com", "payment_request_success_responder.js",
                      &b_method_name);

    NavigateTo("c.com", "/payment_request_can_make_payment_metrics_test.html");

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                                 DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                                 DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED,
                                 DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::DIALOG_OPENED});
    ASSERT_EQ("success",
              content::EvalJs(GetActiveWebContents(),
                              content::JsReplace(
                                  "queryShowWithMethods([{supportedMethods:$1}"
                                  ", {supportedMethods:$2}])",
                                  a_method_name, b_method_name)));
    WaitForObservedEvent();

    // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
    // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
    base::RunLoop().RunUntilIdle();
  }

  void InstallTwoPaymentHandlersAndNoQueryShow() {
    std::string a_method_name;
    InstallPaymentApp("a.com", "payment_request_success_responder.js",
                      &a_method_name);

    std::string b_method_name;
    InstallPaymentApp("b.com", "payment_request_success_responder.js",
                      &b_method_name);

    NavigateTo("c.com", "/payment_request_can_make_payment_metrics_test.html");

    ResetEventWaiterForDialogOpened();
    ASSERT_EQ("success", content::EvalJs(
                             GetActiveWebContents(),
                             content::JsReplace(
                                 "noQueryShowWithMethods([{supportedMethods:$1}"
                                 ", {supportedMethods:$2}])",
                                 a_method_name, b_method_name)));
    WaitForObservedEvent();

    // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
    // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_True_NotShown) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  base::HistogramTester histogram_tester;

  std::string a_method_name;
  InstallPaymentApp("a.com", "can_make_payment_true_responder.js",
                    &a_method_name);

  NavigateTo("b.com", "/payment_request_can_make_payment_metrics_test.html");

  // Try to start the Payment Request, but only check payment support without
  // calling show().
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("queryNoShowWithMethods([{supportedMethods:$1}]);",
                             a_method_name)));

  // Navigate away to trigger the log.
  NavigateTo("a.com", "/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_False_NotShown) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Try to start the Payment Request, but only check payment support without
  // calling show(). Query unenrolled and not JIT methods.
  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "queryNoShowWithUrlMethods();"));
  WaitForObservedEvent();

  // Navigate away to trigger the log.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_False_Shown_Completed) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  std::string nickpay_method_name =
      nickpay_server_.GetURL("nickpay.com", "/pay").spec();
  std::string nickpay2_method_name =
      nickpay_server_.GetURL("nickpay2.com", "/pay").spec();

  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED});
  // Install payment apps JIT, so HasEnrolledInstrument returns false.
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("queryShowWithMethods([{supportedMethods:$1}"
                                   ",{supportedMethods:$2}]);",
                                   nickpay_method_name, nickpay2_method_name)));
  WaitForObservedEvent();

  // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
  // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
  base::RunLoop().RunUntilIdle();

  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_False_Shown_OtherAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  std::string nickpay_method_name =
      nickpay_server_.GetURL("nickpay.com", "/pay").spec();
  std::string nickpay2_method_name =
      nickpay_server_.GetURL("nickpay2.com", "/pay").spec();

  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED});
  // Install payment apps JIT, so HasEnrolledInstrument returns false.
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("queryShowWithMethods([{supportedMethods:$1}"
                                   ",{supportedMethods:$2}]);",
                                   nickpay_method_name, nickpay2_method_name)));
  WaitForObservedEvent();

  // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
  // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
  base::RunLoop().RunUntilIdle();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_False_Shown_UserAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  std::string nickpay_method_name =
      nickpay_server_.GetURL("nickpay.com", "/pay").spec();
  std::string nickpay2_method_name =
      nickpay_server_.GetURL("nickpay2.com", "/pay").spec();

  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED});
  // Install payment apps JIT, so HasEnrolledInstrument returns false.
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("queryShowWithMethods([{supportedMethods:$1}"
                                   ",{supportedMethods:$2}]);",
                                   nickpay_method_name, nickpay2_method_name)));
  WaitForObservedEvent();

  // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
  // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
  base::RunLoop().RunUntilIdle();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_True_Shown_Completed) {
  base::HistogramTester histogram_tester;

  std::string a_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &a_method_name);

  NavigateTo("b.com", "/payment_request_can_make_payment_metrics_test.html");

  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "queryShowWithMethodsBlocking([{supportedMethods:$1}]);",
                    a_method_name)));

  // Navigate away to trigger the log.
  NavigateTo("a.com", "/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  // "EVENT_SHOWN" fires only when the browser payment sheet shows and waits
  // for user to click "Continue", but this test skips the browser payment
  // sheet to launch directly into the payment app.
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_True_Shown_OtherAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    Called_True_Shown_UserAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    NotCalled_Shown_Completed) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndNoQueryShow();

  // Complete the Payment Request.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    NotCalled_Shown_OtherAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndNoQueryShow();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    NotCalled_Shown_UserAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndNoQueryShow();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    UserAborted_NavigationToDifferentOrigin) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on a different origin.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  GURL other_origin_url =
      https_server()->GetURL("b.com", "/payment_request_email_test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_origin_url));
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    UserAborted_NavigationToSameOrigin) {
  base::HistogramTester histogram_tester;

  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on the same origin.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  NavigateTo("c.com", "/payment_request_email_test.html");
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    UserAborted_Reload) {
  base::HistogramTester histogram_tester;

  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user reloads the page containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestCanMakePaymentMetricsWithBasicCardDisabledTest,
    UserAborted_TabClose) {
  base::HistogramTester histogram_tester;

  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user closes the tab containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::CloseTab(browser());
  WaitForObservedEvent();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_COMPLETED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE);
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_TRUE);
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}
}  // namespace payments
