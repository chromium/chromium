// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace payments {

namespace {

using Event2 = payments::JourneyLogger::Event2;

int toInt(Event2 event) {
  return static_cast<int>(event);
}

}  // namespace

using PaymentRequestCompletionStatusMetricsTest = PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest, Completed) {
  base::HistogramTester histogram_tester;

  std::string method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &method_name);

  NavigateTo("b.com", "/payment_request_can_make_payment_metrics_test.html");

  // Try to start the Payment Request and call show().
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "queryShowWithMethodsBlocking([{supportedMethods:$1}]);",
                    method_name)));

  // Navigate away to trigger the log.
  NavigateTo("a.com", "/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
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

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       MerchantAborted_Reload) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventWaiterForDialogOpened();
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "noQueryShowWithMethods([{supportedMethods:$1}"
                                ", {supportedMethods:$2}])",
                                a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // The merchant reloads the page.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "(function() { location.reload(); })();"));
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       MerchantAborted_Navigation) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventWaiterForDialogOpened();
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "noQueryShowWithMethods([{supportedMethods:$1}"
                                ", {supportedMethods:$2}])",
                                a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // The merchant navigates away.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "(function() { window.location.href = "
                              "'/payment_request_email_test.html'; "
                              "})();"));
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       MerchantAborted_Abort) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);

  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);
  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventWaiterForDialogOpened();
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "noQueryShowWithMethods([{supportedMethods:$1}"
                                ", {supportedMethods:$2}])",
                                a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // The merchant aborts the Payment Request.
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
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       UserAborted_Navigation) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventWaiterForDialogOpened();
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "noQueryShowWithMethods([{supportedMethods:$1}"
                                ", {supportedMethods:$2}])",
                                a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Navigate away.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

// TODO(crbug.com/40866418): Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       DISABLED_UserAborted_CancelButton) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventWaiterForDialogOpened();
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "noQueryShowWithMethods([{supportedMethods:$1}"
                                ", {supportedMethods:$2}])",
                                a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Click on the cancel button.
  ClickOnCancel();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

// TODO(crbug.com/40866418): Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       DISABLED_UserAborted_TabClosed) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventWaiterForDialogOpened();
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "noQueryShowWithMethods([{supportedMethods:$1}"
                                ", {supportedMethods:$2}])",
                                a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Close the tab containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::CloseTab(browser());
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       UserAborted_Reload) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_can_make_payment_metrics_test.html");
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventWaiterForDialogOpened();
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "noQueryShowWithMethods([{supportedMethods:$1}"
                                ", {supportedMethods:$2}])",
                                a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Reload the page containing the Payment Request.
  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(WaitForObservedEvent());

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kPayClicked));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
}

using PaymentRequestInitiatedCompletionStatusMetricsTest =
    PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestInitiatedCompletionStatusMetricsTest,
                       Aborted_NotShown) {
  base::HistogramTester histogram_tester;
  NavigateTo("/initiated_test.html");

  // Ensure that the browser side PaymentRequest service has initialized.
  EXPECT_EQ(false, content::EvalJs(
                       GetActiveWebContents(),
                       content::JsReplace(
                           "canMakePayment($1)",
                           https_server()->GetURL("example.test", "/webpay"))));

  // Navigate away.
  NavigateTo("/payment_request_email_test.html");

  // There is one sample, because the request was initiated.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(toInt(Event2::kInitiated) | toInt(Event2::kUserAborted) |
                toInt(Event2::kRequestMethodOther),
            buckets[0].min);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       PaymentRequestRowViewAccessibleName) {
  auto payment_view = std::make_unique<PaymentRequestRowView>();
  auto label1 = std::make_unique<views::Label>(u"Label 1");
  auto label2 = std::make_unique<views::Label>(u"Label 2");
  auto label3 = std::make_unique<views::Label>(u"Label 3");
  auto label4 = std::make_unique<views::Label>(u"Label 4");

  payment_view->AddChildView(label1.get());
  ui::AXNodeData data;
  payment_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"Label 1",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  label1->AddChildView(label2.get());
  data = ui::AXNodeData();
  payment_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"Label 1",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  payment_view->AddChildView(label3.get());
  payment_view->AddChildView(label4.get());
  data = ui::AXNodeData();
  payment_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"Label 1\nLabel 3\nLabel 4",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  auto payment_button = std::make_unique<PaymentRequestRowView>();
  auto label5 = std::make_unique<views::Label>(u"Label 5");
  payment_button->AddChildView(label5.get());

  data = ui::AXNodeData();
  payment_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"Label 5",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  payment_view->AddChildView(payment_button.get());
  data = ui::AXNodeData();
  payment_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"Label 1\nLabel 3\nLabel 4",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

}  // namespace payments
