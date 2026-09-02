// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/payment_app_loading_view.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/payments/core/features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/styled_label.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#endif

namespace payments {
namespace {

using ::testing::UnorderedElementsAre;

class PaymentRequestTest : public PaymentRequestBrowserTestBase {};

// If the page creates multiple PaymentRequest objects, it should not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestTest, MultipleRequests) {
  NavigateTo("/payment_request_multiple_requests.html");
  const std::vector<PaymentRequest*> payment_requests = GetPaymentRequests();
  EXPECT_EQ(5U, payment_requests.size());
}

class PaymentRequestNoShippingTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestNoShippingTest(const PaymentRequestNoShippingTest&) = delete;
  PaymentRequestNoShippingTest& operator=(const PaymentRequestNoShippingTest&) =
      delete;

 protected:
  PaymentRequestNoShippingTest() = default;

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

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateTo404) {
  OpenPaymentRequestDialog();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  NavigateTo("/non-existent.html");
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateToSame) {
  OpenPaymentRequestDialog();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  NavigateTo("/payment_request_no_shipping_test.html");
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndReload) {
  OpenPaymentRequestDialog();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndClickCancel) {
  OpenPaymentRequestDialog();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest,
                       OrderSummaryAndClickCancel) {
  OpenPaymentRequestDialog();
  OpenOrderSummaryScreen();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

// TODO(crbug.com/40924925): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest,
                       DISABLED_InactiveBrowserWindow) {
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  SetBrowserWindowInactive();

  EXPECT_EQ(
      "Cannot show PaymentRequest UI in a preview page or a background tab.",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
              a_method_name, b_method_name)));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, InvalidSSL) {
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");
  SetInvalidSsl();

  EXPECT_EQ(
      "Invalid SSL certificate",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
              a_method_name, b_method_name)));
}

using PaymentRequestAbortTest = PaymentRequestBrowserTestBase;

// Testing the use of the abort() JS API.
IN_PROC_BROWSER_TEST_F(PaymentRequestAbortTest, OpenThenAbort) {
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

  // The web-modal dialog should now be closed.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

using PaymentRequestPaymentMethodIdentifierTest = PaymentRequestBrowserTestBase;

// A url-based payment method identifier is only supported if it has an https
// scheme.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentMethodIdentifierTest, Url_Valid) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_payment_method_identifier_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyHelper([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));

  std::vector<PaymentRequest*> requests = GetPaymentRequests();
  EXPECT_EQ(1u, requests.size());
  std::vector<GURL> url_payment_method_identifiers =
      requests[0]->spec()->url_payment_method_identifiers();
  EXPECT_EQ(2u, url_payment_method_identifiers.size());
  EXPECT_EQ("https://", url_payment_method_identifiers[0].spec().substr(0, 8));
}

// Test harness integrating with DialogBrowserTest to present the dialog in an
// interactive manner for visual testing.
class PaymentsRequestVisualTest
    : public SupportsTestDialog<PaymentRequestNoShippingTest> {
 public:
  PaymentsRequestVisualTest(const PaymentsRequestVisualTest&) = delete;
  PaymentsRequestVisualTest& operator=(const PaymentsRequestVisualTest&) =
      delete;

 protected:
  PaymentsRequestVisualTest() = default;

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override {
    InvokePaymentRequestUIWithJs(content::JsReplace(
        "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
        a_method_name_, b_method_name_));
  }

  bool AlwaysCloseAsynchronously() override {
    // Bypassing Widget::CanClose() causes payments::JourneyLogger to see the
    // show, but not the close, resulting in a DCHECK in its destructor.
    return true;
  }

  std::string a_method_name_;
  std::string b_method_name_;
};

IN_PROC_BROWSER_TEST_F(PaymentsRequestVisualTest, InvokeUi_NoShipping) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name_);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name_);

  NavigateTo("/payment_request_no_shipping_test.html");
  ShowAndVerifyUi();
}

using PaymentRequestSettingsLinkTest = PaymentRequestBrowserTestBase;

// Tests that clicking the settings link brings the user to settings.
IN_PROC_BROWSER_TEST_F(PaymentRequestSettingsLinkTest, ClickSettingsLink) {
#if BUILDFLAG(IS_CHROMEOS)
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser()->GetProfile())
      ->InstallSystemAppsForTesting();
#endif

  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_no_shipping_test.html");

  // Click on the settings link in the payment request dialog window.
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  views::StyledLabel* styled_label =
      static_cast<views::StyledLabel*>(dialog_view()->GetViewByID(
          static_cast<int>(DialogViewID::DATA_SOURCE_LABEL)));
  EXPECT_TRUE(styled_label);
  content::WebContentsAddedObserver web_contents_added_observer;
  styled_label->ClickFirstLinkForTesting();
  content::WebContents* new_tab_contents =
      web_contents_added_observer.GetWebContents();

  EXPECT_EQ(
      std::string(chrome::kChromeUISettingsURL) + chrome::kPaymentsSubPage,
      new_tab_contents->GetVisibleURL().spec());
}

class PaymentRequestMandatoryUiEnabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestMandatoryUiEnabledTest() {
    feature_list_.InitAndEnableFeature(
        payments::features::kPaymentRequestMandatoryPaymentAppUi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestMandatoryUiEnabledTest, AsyncCloseDialog) {
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

  // The dialog is open, so we should have 1 payment request.
  EXPECT_EQ(1U, GetPaymentRequests().size());

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  // Call CloseDialog, it should asynchronously close widget.
  dialog_view()->CloseDialog();
  // Since close dialog is async, the PaymentRequest should still be alive
  // immediately after the call.
  EXPECT_EQ(1U, GetPaymentRequests().size());
  ASSERT_TRUE(WaitForObservedEvent());

  // Now the PaymentRequest should be deleted.
  EXPECT_TRUE(GetPaymentRequests().empty());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMandatoryUiEnabledTest,
                       ShowAndHideLoadingView) {
  base::HistogramTester histogram_tester;
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

  EXPECT_EQ(1U, GetPaymentRequests().size());
  EXPECT_TRUE(test_api(dialog_view()).view_stack()->GetVisible());
  EXPECT_EQ(nullptr, test_api(dialog_view()).loading_view_overlay());

  ResetEventWaiter(DialogEvent::LOADING_VIEW_SHOWN);
  dialog_view()->ShowLoadingView();
  ASSERT_TRUE(WaitForObservedEvent());

  PaymentAppLoadingView* loading_view =
      test_api(dialog_view()).loading_view_overlay();
  ASSERT_NE(nullptr, loading_view);
  EXPECT_TRUE(loading_view->GetVisible());
  EXPECT_FALSE(test_api(dialog_view()).view_stack()->GetVisible());

  ResetEventWaiter(DialogEvent::LOADING_VIEW_HIDDEN);
  dialog_view()->HideLoadingView();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_EQ(nullptr, test_api(dialog_view()).loading_view_overlay());
  EXPECT_TRUE(test_api(dialog_view()).view_stack()->GetVisible());

  histogram_tester.ExpectTotalCount(
      "PaymentRequest.MandatoryPaymentAppUi.LoadingViewShownDuration.Completed",
      1);
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.MandatoryPaymentAppUi.LoadingViewShownDuration.Aborted",
      0);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestMandatoryUiEnabledTest,
                       ShowAndAbortLoadingView) {
  base::HistogramTester histogram_tester;
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

  EXPECT_EQ(1U, GetPaymentRequests().size());
  EXPECT_TRUE(test_api(dialog_view()).view_stack()->GetVisible());
  EXPECT_EQ(nullptr, test_api(dialog_view()).loading_view_overlay());

  ResetEventWaiter(DialogEvent::LOADING_VIEW_SHOWN);
  dialog_view()->ShowLoadingView();
  ASSERT_TRUE(WaitForObservedEvent());

  PaymentAppLoadingView* loading_view =
      test_api(dialog_view()).loading_view_overlay();
  ASSERT_NE(nullptr, loading_view);
  EXPECT_TRUE(loading_view->GetVisible());

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  dialog_view()->CloseDialog();
  ASSERT_TRUE(WaitForObservedEvent());

  histogram_tester.ExpectTotalCount(
      "PaymentRequest.MandatoryPaymentAppUi.LoadingViewShownDuration.Completed",
      0);
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.MandatoryPaymentAppUi.LoadingViewShownDuration.Aborted",
      1);
}

// Hiding the loading view is delayed by a timer. If the dialog closes while
// the timer is still waiting, OnDialogClosed() destroys view_stack_ first.
// When RemoveLoadingView() runs, it normally touches view_stack_ to restore its
// visibility and request focus. This test ensures that RemoveLoadingView()
// safely handles a null/destroyed view_stack_ without crashing.
IN_PROC_BROWSER_TEST_F(PaymentRequestMandatoryUiEnabledTest,
                       RemoveLoadingViewAfterOnDialogClosedDoesNotCrash) {
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

  EXPECT_EQ(1U, GetPaymentRequests().size());
  EXPECT_TRUE(test_api(dialog_view()).view_stack()->GetVisible());
  EXPECT_EQ(nullptr, test_api(dialog_view()).loading_view_overlay());

  ResetEventWaiter(DialogEvent::LOADING_VIEW_SHOWN);
  dialog_view()->ShowLoadingView();
  ASSERT_TRUE(WaitForObservedEvent());

  PaymentAppLoadingView* loading_view =
      test_api(dialog_view()).loading_view_overlay();
  ASSERT_NE(nullptr, loading_view);
  EXPECT_TRUE(loading_view->GetVisible());

  PaymentRequestDialogView* dialog = dialog_view();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  dialog->CloseDialog();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_EQ(nullptr, test_api(dialog).view_stack());

  test_api(dialog).RemoveLoadingView();
  EXPECT_EQ(nullptr, test_api(dialog).loading_view_overlay());
}

}  // namespace
}  // namespace payments
