// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/content/payment_request.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
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
}  // namespace
}  // namespace payments
