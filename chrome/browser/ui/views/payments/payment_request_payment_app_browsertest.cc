// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "components/payments/core/features.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestPaymentAppTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentAppTest()
      : alicepay_(net::EmbeddedTestServer::TYPE_HTTPS),
        bobpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        frankpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        kylepay_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        // enabled features
        {::features::kServiceWorkerPaymentApps,
         features::kAlwaysAllowJustInTimePaymentApp},
        // disabled features
        {});
  }

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  // Starts the test severs and opens a test page on alicepay.com.
  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

    ASSERT_TRUE(StartTestServer("alicepay.com", &alicepay_));
    ASSERT_TRUE(StartTestServer("bobpay.com", &bobpay_));
    ASSERT_TRUE(StartTestServer("frankpay.com", &frankpay_));
    ASSERT_TRUE(StartTestServer("kylepay.com", &kylepay_));

    GetPermissionRequestManager()->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);
  }

  // Invokes the JavaScript function install(|method_name|) in
  // components/test/data/payments/alicepay.com/app1/index.js, which responds
  // back via domAutomationController.
  void InstallAlicePayForMethod(const std::string& method_name) {
    ui_test_utils::NavigateToURL(browser(),
                                 alicepay_.GetURL("alicepay.com", "/app1/"));

    std::string contents;
    std::string script = "install('" + method_name + "');";
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), script,
        &contents))
        << "Script execution failed: " << script;
    ASSERT_NE(std::string::npos,
              contents.find("Payment app for \"" + method_name +
                            "\" method installed."))
        << method_name << " method install message not found in:\n"
        << contents;
  }

  // Invokes the JavaScript function install(|method_name|) in
  // components/test/data/payments/bobpay.com/app1/index.js, which responds
  // back via domAutomationController.
  void InstallBobPayForMethod(const std::string& method_name) {
    ui_test_utils::NavigateToURL(browser(),
                                 bobpay_.GetURL("bobpay.com", "/app1/"));

    std::string contents;
    std::string script = "install('" + method_name + "');";
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), script,
        &contents))
        << "Script execution failed: " << script;
    ASSERT_NE(std::string::npos,
              contents.find("Payment app for \"" + method_name +
                            "\" method installed."))
        << method_name << " method install message not found in:\n"
        << contents;
  }

  // Installs Kyle Pay.
  void InstallKylePay() {
    ui_test_utils::NavigateToURL(browser(),
                                 kylepay_.GetURL("kylepay.com", "/"));
    EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                         "install('https://kylepay.com');"));
  }

  void BlockAlicePay() {
    GURL origin = alicepay_.GetURL("alicepay.com", "/app1/").GetOrigin();
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetContentSettingDefaultScope(origin, origin,
                                        ContentSettingsType::PAYMENT_HANDLER,
                                        CONTENT_SETTING_BLOCK);
  }

  // Sets a TestDownloader for ServiceWorkerPaymentAppFinder and ignores port in
  // app scope. Must be called while on the page that will invoke the
  // PaymentRequest API, because ServiceWorkerPaymentAppFinder is owned by the
  // page.
  void SetDownloaderAndIgnorePortInOriginComparisonForTesting() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(
            web_contents->GetBrowserContext())
            ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://alicepay.com/",
                                 alicepay_.GetURL("alicepay.com", "/"));
    downloader->AddTestServerURL("https://bobpay.com/",
                                 bobpay_.GetURL("bobpay.com", "/"));
    downloader->AddTestServerURL("https://frankpay.com/",
                                 frankpay_.GetURL("frankpay.com", "/"));
    downloader->AddTestServerURL("https://kylepay.com/",
                                 kylepay_.GetURL("kylepay.com", "/"));
    ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(
        web_contents->GetMainFrame())
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));
  }

 private:
  // Starts the |test_server| for |hostname|. Returns true on success.
  bool StartTestServer(const std::string& hostname,
                       net::EmbeddedTestServer* test_server) {
    host_resolver()->AddRule(hostname, "127.0.0.1");
    if (!test_server->InitializeAndListen())
      return false;
    test_server->ServeFilesFromSourceDirectory(
        "components/test/data/payments/" + hostname);
    test_server->StartAcceptingConnections();
    return true;
  }

  // https://alicepay.com hosts the payment app.
  net::EmbeddedTestServer alicepay_;

  // https://bobpay.com/webpay does not permit any other origin to use this
  // payment method.
  net::EmbeddedTestServer bobpay_;

  // https://frankpay.com/webpay supports payment apps from any origin.
  net::EmbeddedTestServer frankpay_;

  // https://kylepay.com/webpay hosts a just-in-time installable payment app.
  net::EmbeddedTestServer kylepay_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestPaymentAppTest);
};

// Test payment request methods are not supported by the payment app.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, NotSupportedError) {
  InstallAlicePayForMethod("https://frankpay.com");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::NOT_SUPPORTED_ERROR});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }

  // Repeat should have identical results.
  {
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::NOT_SUPPORTED_ERROR});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }
}

// Test CanMakePayment and payment request can be fullfiled.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithAlicePay) {
  InstallAlicePayForMethod("https://alicepay.com");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"true"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();
    ExpectBodyContains({"https://alicepay.com"});
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"true"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();
    ExpectBodyContains({"https://alicepay.com"});
  }
}

// Test CanMakePayment and payment request can be fullfiled in incognito mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithAlicePayIncognito) {
  SetIncognito();
  InstallAlicePayForMethod("https://alicepay.com");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"true"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();
    ExpectBodyContains({"https://alicepay.com"});
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"true"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();
    ExpectBodyContains({"https://alicepay.com"});
  }
}

// Test payment apps are not available if they are blocked.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, BlockAlicePay) {
  InstallAlicePayForMethod("https://alicepay.com");
  BlockAlicePay();

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::NOT_SUPPORTED_ERROR});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::NOT_SUPPORTED_ERROR});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }
}

// Test https://bobpay.com can not be used by https://alicepay.com
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, CanNotPayWithBobPay) {
  InstallAlicePayForMethod("https://bobpay.com");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::NOT_SUPPORTED_ERROR});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
    ExpectBodyContains({"false"});

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::NOT_SUPPORTED_ERROR});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
    ExpectBodyContains({"NotSupportedError"});
  }
}

// Test can pay with 'basic-card' payment method from alicepay.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithBasicCard) {
  InstallAlicePayForMethod("basic-card");

  {
    NavigateTo(
        "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();
    ExpectBodyContains({"basic-card"});
  }

  // Repeat should have identical results.
  {
    NavigateTo(
        "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();
    ExpectBodyContains({"basic-card"});
  }
}

// Test can cancel payment with 'basic-card' payment method from alicepay.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithBasicCardCancel) {
  // Install both alicepay and bobpay to force showing payment sheet.
  InstallAlicePayForMethod("basic-card");
  InstallBobPayForMethod("https://bobpay.com");

  {
    NavigateTo(
        "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();
    InvokePaymentRequestUI();
    ClickOnCancel();
    ExpectBodyContains({"User closed the Payment Request UI."});
  }
  // Repeat should have identical results.
  {
    NavigateTo(
        "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();
    InvokePaymentRequestUI();
    ClickOnCancel();
    ExpectBodyContains({"User closed the Payment Request UI."});
  }
}

class PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip
    : public PaymentRequestPaymentAppTest {
 public:
  PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip() {
    feature_list_.InitWithFeatures(
        {
            payments::features::kWebPaymentsSingleAppUiSkip,
            ::features::kServiceWorkerPaymentApps,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
                       SkipUIEnabledWithBobPay) {
  base::HistogramTester histogram_tester;
  InstallBobPayForMethod("https://bobpay.com");

  {
    NavigateTo("/payment_request_bobpay_ui_skip_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    // Since the skip UI flow is available, the request will complete without
    // interaction besides hitting "pay" on the website.
    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();

    ExpectBodyContains({"bobpay.com"});

    histogram_tester.ExpectTotalCount("PaymentRequest.TimeToCheckout.Completed",
                                      1);
    histogram_tester.ExpectTotalCount(
        "PaymentRequest.TimeToCheckout.Completed.SkippedShow", 1);
    histogram_tester.ExpectTotalCount(
        "PaymentRequest.TimeToCheckout.Completed.SkippedShow.Other", 1);
  }
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
    SkipUIDEnabledWithSingleAvailableAppAndMultipleAcceptedMethods) {
  InstallBobPayForMethod("https://bobpay.com");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    // Even though both bobpay.com and alicepay.com methods are requested, since
    // only bobpay is installed skip UI is enabled.
    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();

    ExpectBodyContains({"bobpay.com"});
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
                       SkipUIDisabledWithMultipleAvailableApp) {
  InstallBobPayForMethod("https://bobpay.com");
  InstallAlicePayForMethod("https://alicepay.com");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    // Skip UI is disabled since both bobpay.com and alicepay.com methods are
    // requested and both apps are installed.
    ResetEventWaiterForDialogOpened();
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy()"));
    WaitForObservedEvent();

    // Click on pay.
    EXPECT_TRUE(IsPayButtonEnabled());
    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

    // Depending on which installation completes first the preselected app can
    // be either bobpay or alicepay. Regardless of which app completed the
    // request both include "transactionId: '123'" in their responses.
    ExpectBodyContains({"\"transactionId\": \"123\""});
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
                       SkipUIEnabledWhenSingleAppCanProvideAllInfo) {
  InstallBobPayForMethod("https://bobpay.com");
  InstallKylePay();

  {
    NavigateTo("/payment_request_bobpay_and_cards_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    // Even though two methods are requested and both apps are installed, skip
    // UI is enabled since only KylePay can provide all requested information
    // including shipping address and payer's contact info.
    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN,
         DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
         DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ASSERT_TRUE(content::ExecJs(
        GetActiveWebContents(),
        "testPaymentMethods([{supportedMethods: 'https://bobpay.com'}, "
        "{supportedMethods: 'https://kylepay.com'}], true /*= "
        "requestShippingContact */)"));
    WaitForObservedEvent();

    ExpectBodyContains({"kylepay.com"});
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
                       SkipUIDisabledWithRequestedPayerEmail) {
  InstallBobPayForMethod("https://bobpay.com");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);

  {
    NavigateTo("/payment_request_bobpay_ui_skip_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    // Since the skip UI flow is not available because the payer's email is
    // requested and bobpay cannot proivde it, the request will complete only
    // after clicking on the Pay button in the dialog.
    ResetEventWaiterForDialogOpened();
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_buy_button_js =
        "(function() { "
        "document.getElementById('buyWithRequestedEmail').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
    WaitForObservedEvent();
    EXPECT_TRUE(IsPayButtonEnabled());

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

    ExpectBodyContains({"bobpay.com"});
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest,
                       AlwaysAllowJustInTimeInstall) {
  // Add a complete card to ensure that autofill payment app is available.
  const autofill::CreditCard card = autofill::test::GetCreditCard();
  AddCreditCard(card);

  // Trigger a request that specifies both kylepay.com and basic-card.
  NavigateTo("/payment_request_bobpay_and_cards_test.html");
  SetDownloaderAndIgnorePortInOriginComparisonForTesting();

  ResetEventWaiterForDialogOpened();
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "testInstallableAppAndCard();"));
  WaitForObservedEvent();

  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains({"kylepay.com/webpay"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest,
                       ReadSupportedDelegationsFromAppManifest) {
  // Trigger a request that specifies kylepay.com and asks for shipping address
  // as well as payer's contact information. kylepay.com hosts an installable
  // payment app which handles both shipping address and payer's contact
  // information.
  NavigateTo("/payment_request_bobpay_and_cards_test.html");
  SetDownloaderAndIgnorePortInOriginComparisonForTesting();
  ResetEventWaiterForDialogOpened();
  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.com/webpay'}], "
      "true /*= requestShippingContact */);"));
  WaitForObservedEvent();

  // Pay button should be enabled without any autofill profiles since the
  // selected payment instrument (kylepay) handles all merchant required
  // information.
  EXPECT_TRUE(IsPayButtonEnabled());

  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains({"kylepay.com/webpay"});
}
}  // namespace payments
