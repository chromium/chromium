// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/journey_logger.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace payments {

namespace {

using Event2 = payments::JourneyLogger::Event2;

int toInt(Event2 event) {
  return static_cast<int>(event);
}

}  // namespace

using PaymentRequestJourneyLoggerSelectedPaymentAppTest =
    PaymentRequestBrowserTestBase;

// Tests that the selected app metric is correctly logged when the
// Payment Request is completed with a payment handler.
IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerSelectedPaymentAppTest,
                       TestSelectedPaymentMethod) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  base::HistogramTester histogram_tester;

  // Complete the Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

using PaymentRequestJourneyLoggerNoSupportedPaymentMethodTest =
    PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerNoSupportedPaymentMethodTest,
                       OnlyBobpaySupported) {
  NavigateTo("/payment_request_bobpay_test.html");
  base::HistogramTester histogram_tester;

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::NOT_SUPPORTED_ERROR});
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('buy').click(); })();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_buy_button_js));
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

using PaymentRequestJourneyLoggerMultipleShowTest =
    PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerMultipleShowTest,
                       ShowSameRequest) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("c.com", "/payment_request_multiple_show_test.html");

  base::HistogramTester histogram_tester;

  // Start a Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  // Try to show it again.
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('showAgain').click(); })();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_buy_button_js));

  // Complete the original Payment Request.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Trying to show the same request twice is not considered a concurrent
  // request.
  EXPECT_TRUE(
      histogram_tester.GetAllSamples("PaymentRequest.CheckoutFunnel.NoShow")
          .empty());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerMultipleShowTest,
                       StartNewRequest) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_multiple_show_test.html");

  base::HistogramTester histogram_tester;

  // Start a Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  // Get the dialog view of the first request, since the one cached for the
  // tests will be replaced by the second Payment Request.
  PaymentRequestDialogView* first_dialog_view = dialog_view();

  // Try to show a second request.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace("showSecondRequestWithMethods([{supportedMethods:$1}, "
                         "{supportedMethods:$2}]);",
                         a_method_name, b_method_name)));

  // Complete the original Payment Request.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, first_dialog_view);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(2U, buckets.size());
  base::HistogramBase::Sample shown;
  base::HistogramBase::Sample could_not_show;
  // Order of histogram recording is non-deterministic. So use EVENT_SHOWN bit
  // to differentiate between the two histograms.
  if (buckets[0].min & toInt(Event2::kShown)) {
    shown = buckets[0].min;
    could_not_show = buckets[1].min;
  } else {
    shown = buckets[1].min;
    could_not_show = buckets[0].min;
  }
  // This is for the first (completed) request.
  EXPECT_TRUE(shown & toInt(Event2::kShown));
  EXPECT_TRUE(shown & toInt(Event2::kPayClicked));
  EXPECT_FALSE(shown & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(shown & toInt(Event2::kCompleted));
  EXPECT_FALSE(shown & toInt(Event2::kUserAborted));
  EXPECT_FALSE(shown & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(shown & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(shown & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(shown & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(shown & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(shown & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(shown & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(shown & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(shown & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(shown & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(shown & toInt(Event2::kCouldNotShow));

  // The second set of events is for the second (never shown) request.
  EXPECT_FALSE(could_not_show & toInt(Event2::kShown));
  EXPECT_FALSE(could_not_show & toInt(Event2::kPayClicked));
  EXPECT_FALSE(could_not_show & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(could_not_show & toInt(Event2::kCompleted));
  EXPECT_FALSE(could_not_show & toInt(Event2::kUserAborted));
  EXPECT_FALSE(could_not_show & toInt(Event2::kOtherAborted));

  // The checkout funnel is aborted before checking for initial form of payment
  // and the necessary complete suggestions, because querying for available
  // payment apps is asynchronous.
  EXPECT_FALSE(could_not_show & toInt(Event2::kHadInitialFormOfPayment));

  EXPECT_FALSE(could_not_show & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(could_not_show & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(could_not_show & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(could_not_show & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(could_not_show & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(could_not_show & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(could_not_show & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(could_not_show & toInt(Event2::kSelectedOther));
  EXPECT_TRUE(could_not_show & toInt(Event2::kCouldNotShow));
}

using PaymentRequestJourneyLoggerAllSectionStatsTest =
    PaymentRequestBrowserTestBase;

// Tests that the correct PaymentRequest.Events metrics are logged when a
// Payment Request is completed.
IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerAllSectionStatsTest,
                       EventsMetric_Completed) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);

  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("c.com",
             "/payment_request_contact_details_and_free_shipping_test.html");
  base::HistogramTester histogram_tester;

  // Add two addresses.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // Complete the Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

// Tests that the correct PaymentRequest.Events metrics are logged when a
// Payment Request is aborted by the user.
IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerAllSectionStatsTest,
                       EventsMetric_UserAborted) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);

  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("c.com",
             "/payment_request_contact_details_and_free_shipping_test.html");
  base::HistogramTester histogram_tester;

  // Add two addresses and contact infos.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // The user aborts the Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

using PaymentRequestJourneyLoggerNoShippingSectionStatsTest =
    PaymentRequestBrowserTestBase;

// Tests that the correct PaymentRequest.Events metrics are logged when a
// Payment Request is completed.
IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerNoShippingSectionStatsTest,
                       EventsMetric_Completed) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("c.com", "/payment_request_contact_details_test.html");
  base::HistogramTester histogram_tester;

  // Add two addresses and contact infos.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // Complete the Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

// Tests that the correct PaymentRequest.Events metrics are logged when a
// Payment Request is aborted by the user.
IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerNoShippingSectionStatsTest,
                       EventsMetric_UserAborted) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("c.com", "/payment_request_contact_details_test.html");
  base::HistogramTester histogram_tester;

  // Add two confact info.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // The user aborts the Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

using PaymentRequestJourneyLoggerNoContactDetailSectionStatsTest =
    PaymentRequestBrowserTestBase;

// Tests that the correct PaymentRequest.Events metrics are logged when a
// Payment Request is completed.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestJourneyLoggerNoContactDetailSectionStatsTest,
    EventsMetric_Completed) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("c.com", "/payment_request_free_shipping_test.html");
  base::HistogramTester histogram_tester;

  // Add two addresses.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // Complete the Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

// Tests that the correct PaymentRequest.Events metrics are logged when a
// Payment Request is aborted by the user.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestJourneyLoggerNoContactDetailSectionStatsTest,
    EventsMetric_UserAborted) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_free_shipping_test.html");
  base::HistogramTester histogram_tester;

  // Add two addresses.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // The user aborts the Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

using PaymentRequestNotShownTest = PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestNotShownTest, OnlyNotShownMetricsLogged) {
  // Installs two apps so that canMakePayment is true.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});

  // Initiate a Payment Request without showing it.
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("queryNoShowWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name)));

  ASSERT_TRUE(WaitForObservedEvent());

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Some events should be logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(toInt(Event2::kUserAborted) |
                toInt(Event2::kHadInitialFormOfPayment) |
                toInt(Event2::kRequestMethodOther),
            buckets[0].min);
}

using PaymentRequestCompleteSuggestionsForEverythingTest =
    PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestCompleteSuggestionsForEverythingTest,
                       UserHadCompleteSuggestionsForEverything) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_email_test.html");
  base::HistogramTester histogram_tester;

  // Add a profile.
  AddAutofillProfile(autofill::test::GetFullProfile());

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  // Navigate away to abort the Payment Request and trigger the logs.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

class PaymentRequestIframeTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestIframeTest(const PaymentRequestIframeTest&) = delete;
  PaymentRequestIframeTest& operator=(const PaymentRequestIframeTest&) = delete;

 protected:
  PaymentRequestIframeTest() = default;

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestIframeTest, CrossOriginIframe) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method);
  std::string b_method;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method);

  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server()->GetURL("c.com", "/payment_request_main.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL iframe_url =
      https_server()->GetURL("d.com", "/payment_request_iframe.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      iframe,
      content::JsReplace("triggerPaymentRequestWithMethods([{supportedMethods:$"
                         "1}, {supportedMethods:$2}])",
                         a_method, b_method)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Simulate that the user cancels the PR.
  ClickOnCancel();

  int64_t expected_step_metric =
      toInt(Event2::kShown) | toInt(Event2::kRequestMethodOther) |
      toInt(Event2::kUserAborted) | toInt(Event2::kHadInitialFormOfPayment);

  // Make sure the correct UMA events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(expected_step_metric, buckets[0].min);

  // Important: Even though the Payment Request is in the iframe, no UKM was
  // logged for the iframe URL, only for the main frame.
  for (const auto& kv : test_ukm_recorder_->GetSources()) {
    EXPECT_NE(iframe_url, kv.second->url());
  }

  // Make sure the UKM was logged correctly.
  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::PaymentRequest_CheckoutEvents::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, main_frame_url);
    EXPECT_EQ(2U, entry->metrics.size());
    test_ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::PaymentRequest_CheckoutEvents::kCompletionStatusName,
        JourneyLogger::COMPLETION_STATUS_USER_ABORTED);
    test_ukm_recorder_->ExpectEntryMetric(
        entry, ukm::builders::PaymentRequest_CheckoutEvents::kEvents2Name,
        expected_step_metric);
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestIframeTest, IframeNavigation_UserAborted) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method);
  std::string b_method;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method);

  NavigateTo("c.com", "/payment_request_free_shipping_with_iframe_test.html");
  base::HistogramTester histogram_tester;

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method, b_method));

  // Make the iframe navigate away.
  EXPECT_TRUE(NavigateIframeToURL(GetActiveWebContents(), "theIframe",
                                  GURL("https://www.example.test")));

  // Simulate that the user cancels the PR.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestIframeTest, IframeNavigation_Completed) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method);
  std::string b_method;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method);

  NavigateTo("/payment_request_free_shipping_with_iframe_test.html");
  base::HistogramTester histogram_tester;

  // Add two addresses.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method, b_method));

  // Make the iframe navigate away.
  EXPECT_TRUE(NavigateIframeToURL(GetActiveWebContents(), "theIframe",
                                  GURL("https://www.example.test")));

  // Complete the Payment Request.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestIframeTest, HistoryPushState_UserAborted) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method);
  std::string b_method;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method);

  NavigateTo("/payment_request_free_shipping_with_iframe_test.html");
  base::HistogramTester histogram_tester;

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method, b_method));

  // PushState on the history.
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "window.history.pushState(\"\", \"\", "
                              "\"/favicon/"
                              "pushstate_with_favicon_pushed.html\");"));

  // Simulate that the user cancels the PR.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestIframeTest, HistoryPushState_Completed) {
  // Installs two apps to ensure that the payment request UI is shown.
  std::string a_method;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method);
  std::string b_method;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method);

  NavigateTo("/payment_request_free_shipping_with_iframe_test.html");
  base::HistogramTester histogram_tester;

  // Add two addresses.
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method, b_method));

  // PushState on the history.
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "window.history.pushState(\"\", \"\", "
                              "\"/favicon/"
                              "pushstate_with_favicon_pushed.html\");"));

  // Complete the Payment Request.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedCreditCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSelectedGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCouldNotShow));
}

}  // namespace payments
