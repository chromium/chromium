// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestMetricsTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestMetricsTest(const PaymentRequestMetricsTest&) = delete;
  PaymentRequestMetricsTest& operator=(const PaymentRequestMetricsTest&) =
      delete;

 protected:
  PaymentRequestMetricsTest() = default;

  void OpenPaymentRequestDialog() {
    // Installs two apps so that the Payment Request UI will be shown.
    std::string a_method_name;
    InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                      &a_method_name);
    std::string b_method_name;
    InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                      &b_method_name);

    NavigateTo("/payment_request_no_shipping_test.html");
    InvokePaymentRequestUIWithJs(content::JsReplace(
        "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
        a_method_name, b_method_name));
  }
};

IN_PROC_BROWSER_TEST_F(PaymentRequestMetricsTest, Success) {
  base::HistogramTester histogram_tester;
  OpenPaymentRequestDialog();
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON,
                           /*wait_for_animation=*/false);
  ASSERT_TRUE(WaitForObservedEvent());
  histogram_tester.ExpectUniqueSample("PaymentRequest.Outcome",
                                      PaymentRequestOutcome::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMetricsTest, AbortedByUser_Cancel) {
  base::HistogramTester histogram_tester;
  OpenPaymentRequestDialog();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
  histogram_tester.ExpectUniqueSample("PaymentRequest.Outcome",
                                      PaymentRequestOutcome::kAbortedByUser, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMetricsTest, AbortedByMerchant) {
  base::HistogramTester histogram_tester;
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_abort_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecJs(web_contents, click_buy_button_js));

  ASSERT_TRUE(WaitForObservedEvent());
  ExpectBodyContains({"Aborted"});

  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Outcome", PaymentRequestOutcome::kAbortedByMerchant, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMetricsTest, AbortedUserNavigation) {
  base::HistogramTester histogram_tester;
  OpenPaymentRequestDialog();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  NavigateTo("/non-existent.html");
  ASSERT_TRUE(WaitForObservedEvent());
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Outcome", PaymentRequestOutcome::kAbortedUserNavigation,
      1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMetricsTest, AbortedMerchantNavigation) {
  base::HistogramTester histogram_tester;
  OpenPaymentRequestDialog();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "window.location.href = '/non-existent.html';"));
  ASSERT_TRUE(WaitForObservedEvent());
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Outcome",
      PaymentRequestOutcome::kAbortedMerchantNavigation, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMetricsTest, NotShownBackgroundTab) {
  base::HistogramTester histogram_tester;
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  SetBrowserWindowInactive();

  EXPECT_EQ(
      "Cannot show PaymentRequest UI in a preview page or a background tab.",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("buyWithMethods([{supportedMethods:$1}]);",
                             a_method_name)));

  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Outcome", PaymentRequestOutcome::kNotShownBackgroundTab,
      1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMetricsTest,
                       NotShownNoSupportedPaymentMethod) {
  base::HistogramTester histogram_tester;
  NavigateTo("/payment_request_no_shipping_test.html");

  EXPECT_TRUE(
      content::EvalJs(
          GetActiveWebContents(),
          "buyWithMethods([{supportedMethods:'https://unsupported.com'}]);")
          .ExtractString()
          .find("The payment method \"https://unsupported.com\" is not "
                "supported") != std::string::npos);

  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Outcome",
      PaymentRequestOutcome::kNotShownNoSupportedPaymentMethod, 1);
}

}  // namespace payments
