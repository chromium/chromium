// Copyright 2017 The Chromium Authors
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

namespace {

using Event2 = payments::JourneyLogger::Event2;

int toInt(Event2 event) {
  return static_cast<int>(event);
}

}  // namespace

class PaymentRequestCanMakePaymentMetricsTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestCanMakePaymentMetricsTest(
      const PaymentRequestCanMakePaymentMetricsTest&) = delete;
  PaymentRequestCanMakePaymentMetricsTest& operator=(
      const PaymentRequestCanMakePaymentMetricsTest&) = delete;

 protected:
  PaymentRequestCanMakePaymentMetricsTest()
      : nickpay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

    // Choosing nickpay for its JIT installation support.
    nickpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/");

    ASSERT_TRUE(nickpay_server_.Start());
  }

  void InstallTwoPaymentHandlersAndQueryShow() {
    std::string a_method_name;
    InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                      &a_method_name);

    std::string b_method_name;
    InstallPaymentApp("b.com", "/payment_request_success_responder.js",
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
    ASSERT_TRUE(WaitForObservedEvent());

    // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
    // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
    base::RunLoop().RunUntilIdle();
  }

  void InstallTwoPaymentHandlersAndNoQueryShow() {
    std::string a_method_name;
    InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                      &a_method_name);

    std::string b_method_name;
    InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                      &b_method_name);

    NavigateTo("c.com", "/payment_request_can_make_payment_metrics_test.html");

    ResetEventWaiterForDialogOpened();
    ASSERT_EQ("success", content::EvalJs(
                             GetActiveWebContents(),
                             content::JsReplace(
                                 "noQueryShowWithMethods([{supportedMethods:$1}"
                                 ", {supportedMethods:$2}])",
                                 a_method_name, b_method_name)));
    ASSERT_TRUE(WaitForObservedEvent());

    // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
    // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
    base::RunLoop().RunUntilIdle();
  }

  net::EmbeddedTestServer nickpay_server_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_NotShown) {
  base::HistogramTester histogram_tester;

  std::string a_method_name;
  InstallPaymentApp("a.com", "/can_make_payment_true_responder.js",
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
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_NotShown) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Try to start the Payment Request, but only check payment support without
  // calling show(). Query unenrolled and not JIT methods.
  ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                               DialogEvent::CAN_MAKE_PAYMENT_RETURNED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
                               DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "queryNoShowWithUrlMethods();"));
  ASSERT_TRUE(WaitForObservedEvent());

  // Navigate away to trigger the log.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_Completed) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  std::string nickpay_method_name =
      nickpay_server_.GetURL("nickpay.test", "/nickpay.test/pay").spec();
  std::string nickpay2_method_name =
      nickpay_server_.GetURL("nickpay2.test", "/nickpay.test/pay").spec();

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
  ASSERT_TRUE(WaitForObservedEvent());

  // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
  // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
  base::RunLoop().RunUntilIdle();

  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_OtherAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  std::string nickpay_method_name =
      nickpay_server_.GetURL("nickpay.test", "/nickpay.test/pay").spec();
  std::string nickpay2_method_name =
      nickpay_server_.GetURL("nickpay2.test", "/nickpay.test/pay").spec();

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
  ASSERT_TRUE(WaitForObservedEvent());

  // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
  // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
  base::RunLoop().RunUntilIdle();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), click_buy_button_js));
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_UserAborted) {
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  std::string nickpay_method_name =
      nickpay_server_.GetURL("nickpay.test", "/nickpay.test/pay").spec();
  std::string nickpay2_method_name =
      nickpay_server_.GetURL("nickpay2.test", "/nickpay.test/pay").spec();

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
  ASSERT_TRUE(WaitForObservedEvent());

  // Flushing the PaymentRequest::AreRequestedMethodsSupportedCallback()
  // callback so that EVENT_SHOWN/SKIPPED_SHOW will be recorded.
  base::RunLoop().RunUntilIdle();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_Completed) {
  base::HistogramTester histogram_tester;

  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
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
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  // "EVENT_SHOWN" fires only when the browser payment sheet shows and waits
  // for user to click "Continue", but this test skips the browser payment
  // sheet to launch directly into the payment app.
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_OtherAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), click_buy_button_js));
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_UserAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_Completed) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndNoQueryShow();

  // Complete the Payment Request.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_OtherAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndNoQueryShow();

  // Simulate that an unexpected error occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), click_buy_button_js));
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_UserAborted) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndNoQueryShow();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure that no canMakePayment events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_NavigationToDifferentOrigin) {
  base::HistogramTester histogram_tester;
  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on a different origin.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  GURL other_origin_url =
      https_server()->GetURL("b.com", "/payment_request_email_test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_origin_url));
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_NavigationToSameOrigin) {
  base::HistogramTester histogram_tester;

  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on the same origin.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  NavigateTo("c.com", "/payment_request_email_test.html");
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_Reload) {
  base::HistogramTester histogram_tester;

  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user reloads the page containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_TabClose) {
  base::HistogramTester histogram_tester;

  InstallTwoPaymentHandlersAndQueryShow();

  // Simulate that the user closes the tab containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::CloseTab(browser());
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}
}  // namespace payments
