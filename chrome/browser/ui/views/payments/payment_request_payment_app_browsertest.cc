// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/payments/payment_app_install_util.h"
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
 public:
  PaymentRequestPaymentAppTest(const PaymentRequestPaymentAppTest&) = delete;
  PaymentRequestPaymentAppTest& operator=(const PaymentRequestPaymentAppTest&) =
      delete;

 protected:
  PaymentRequestPaymentAppTest()
      : alicepay_(net::EmbeddedTestServer::TYPE_HTTPS),
        bobpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        frankpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        kylepay_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  // Starts the test severs.
  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

    ASSERT_TRUE(StartTestServer("alicepay.test", &alicepay_));
    ASSERT_TRUE(StartTestServer("bobpay.test", &bobpay_));
    ASSERT_TRUE(StartTestServer("frankpay.test", &frankpay_));
    ASSERT_TRUE(StartTestServer("kylepay.test", &kylepay_));

    GetPermissionRequestManager()->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);
  }

  void InstallAlicePayForMethod(const std::string& method_name) {
    ASSERT_TRUE(
        PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
            *GetActiveWebContents(),
            alicepay_.GetURL("alicepay.test", "/app1/app.js"), method_name,
            PaymentAppInstallUtil::IconInstall::kWithIcon));
  }

  void InstallBobPayForMethod(const std::string& method_name) {
    ASSERT_TRUE(
        PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
            *GetActiveWebContents(),
            bobpay_.GetURL("bobpay.test", "/app1/app.js"), method_name,
            PaymentAppInstallUtil::IconInstall::kWithIcon));
  }

  void InstallKylePayAndEnableDelegations() {
    ASSERT_TRUE(
        PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
            *GetActiveWebContents(), kylepay_.GetURL("kylepay.test", "/app.js"),
            /*payment_method_identifier=*/"https://kylepay.test",
            PaymentAppInstallUtil::IconInstall::kWithIcon));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), kylepay_.GetURL("kylepay.test", "/")));
    ASSERT_EQ("success",
              content::EvalJs(GetActiveWebContents(), "enableDelegations()"));
  }

  void BlockAlicePay() {
    GURL origin =
        alicepay_.GetURL("alicepay.test", "/app1/").DeprecatedGetOriginAsURL();
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
        GetCSPCheckerForTests(), web_contents->GetBrowserContext()
                                     ->GetDefaultStoragePartition()
                                     ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://alicepay.test/",
                                 alicepay_.GetURL("alicepay.test", "/"));
    downloader->AddTestServerURL("https://bobpay.test/",
                                 bobpay_.GetURL("bobpay.test", "/"));
    downloader->AddTestServerURL("https://frankpay.test/",
                                 frankpay_.GetURL("frankpay.test", "/"));
    downloader->AddTestServerURL("https://kylepay.test/",
                                 kylepay_.GetURL("kylepay.test", "/"));
    ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(
        web_contents->GetPrimaryMainFrame())
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));
  }

 private:
  // Starts the |test_server| for |hostname|. Returns true on success.
  bool StartTestServer(const std::string& hostname,
                       net::EmbeddedTestServer* test_server) {
    host_resolver()->AddRule(hostname, "127.0.0.1");
    if (!test_server->InitializeAndListen()) {
      return false;
    }
    test_server->ServeFilesFromSourceDirectory(
        "components/test/data/payments/" + hostname);
    test_server->StartAcceptingConnections();
    return true;
  }

  // https://alicepay.test hosts the payment app.
  net::EmbeddedTestServer alicepay_;

  // https://bobpay.test does not permit any other origin to use this payment
  // method.
  net::EmbeddedTestServer bobpay_;

  // https://frankpay.test supports payment apps from any origin.
  net::EmbeddedTestServer frankpay_;

  // https://kylepay.test hosts a just-in-time installable payment app.
  net::EmbeddedTestServer kylepay_;
};

// Test payment request methods are not supported by the payment app.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, NotSupportedError) {
  InstallAlicePayForMethod("https://frankpay.test");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("false",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_THAT(
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString(),
        ::testing::HasSubstr(
            "NotSupportedError: The payment methods \"https://alicepay.test\", "
            "\"https://bobpay.test\" are not supported."));
  }

  // Repeat should have identical results.
  {
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    NavigateTo("/payment_request_bobpay_test.html");

    EXPECT_EQ("false",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_THAT(
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString(),
        ::testing::HasSubstr(
            "NotSupportedError: The payment methods \"https://alicepay.test\", "
            "\"https://bobpay.test\" are not supported."));
  }
}

// Test CanMakePayment and payment request can be fulfilled.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithAlicePay) {
  InstallAlicePayForMethod("https://alicepay.test");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("true",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ(
        "https://alicepay.test\n{\n  \"transactionId\": \"123\"\n}",
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString());
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("true",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ(
        "https://alicepay.test\n{\n  \"transactionId\": \"123\"\n}",
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString());
  }
}

// Test CanMakePayment and payment request can be fulfilled in incognito mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, PayWithAlicePayIncognito) {
  SetIncognito();
  InstallAlicePayForMethod("https://alicepay.test");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("true",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ(
        "https://alicepay.test\n{\n  \"transactionId\": \"123\"\n}",
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString());
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("true",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ(
        "https://alicepay.test\n{\n  \"transactionId\": \"123\"\n}",
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString());
  }
}

// Test payment apps are not available if they are blocked.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, BlockAlicePay) {
  InstallAlicePayForMethod("https://alicepay.test");
  BlockAlicePay();

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("false",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));
    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_THAT(
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString(),
        ::testing::HasSubstr(
            "NotSupportedError: The payment methods \"https://alicepay.test\", "
            "\"https://bobpay.test\" are not supported."));
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("false",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_THAT(
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString(),
        ::testing::HasSubstr(
            "NotSupportedError: The payment methods \"https://alicepay.test\", "
            "\"https://bobpay.test\" are not supported."));
  }
}

// Test https://bobpay.test can not be used by https://alicepay.test
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, CanNotPayWithBobPay) {
  InstallAlicePayForMethod("https://bobpay.test");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("false",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_THAT(
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString(),
        ::testing::HasSubstr(
            "NotSupportedError: The payment methods \"https://alicepay.test\", "
            "\"https://bobpay.test\" are not supported."));
  }

  // Repeat should have identical results.
  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_EQ("false",
              content::EvalJs(GetActiveWebContents(), "canMakePayment();"));

    // A new payment request will be created below, so call
    // SetDownloaderAndIgnorePortInOriginComparisonForTesting again.
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    EXPECT_THAT(
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString(),
        ::testing::HasSubstr(
            "NotSupportedError: The payment methods \"https://alicepay.test\", "
            "\"https://bobpay.test\" are not supported."));
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
  InstallBobPayForMethod("https://bobpay.test");

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
    ASSERT_TRUE(WaitForObservedEvent());

    ExpectBodyContains({"bobpay.test"});
  }
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
    SkipUIDEnabledWithSingleAvailableAppAndMultipleAcceptedMethods) {
  InstallBobPayForMethod("https://bobpay.test");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    // Even though both bobpay.test and alicepay.test methods are requested,
    // since only bobpay is installed skip UI is enabled.
    EXPECT_EQ(
        "https://bobpay.test\n{\n  \"transactionId\": \"123\"\n}",
        content::EvalJs(GetActiveWebContents(), "buy();").ExtractString());
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
                       SkipUIDisabledWithMultipleAvailableApp) {
  InstallBobPayForMethod("https://bobpay.test");
  InstallAlicePayForMethod("https://alicepay.test");

  {
    NavigateTo("/payment_request_bobpay_test.html");
    SetDownloaderAndIgnorePortInOriginComparisonForTesting();

    // Skip UI is disabled since both bobpay.test and alicepay.test methods are
    // requested and both apps are installed.
    ResetEventWaiterForDialogOpened();
    content::ExecuteScriptAsync(GetActiveWebContents(), "buy()");
    ASSERT_TRUE(WaitForObservedEvent());

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
  InstallBobPayForMethod("https://bobpay.test");
  InstallKylePayAndEnableDelegations();

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
        "testPaymentMethods([{supportedMethods: 'https://bobpay.test'}, "
        "{supportedMethods: 'https://kylepay.test'}], true /*= "
        "requestShippingContact */)"));
    ASSERT_TRUE(WaitForObservedEvent());

    ExpectBodyContains({"kylepay.test"});
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTestWithPaymentHandlersAndUiSkip,
                       SkipUIDisabledWithRequestedPayerEmail) {
  InstallBobPayForMethod("https://bobpay.test");
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
    ASSERT_TRUE(content::ExecJs(web_contents, click_buy_button_js));
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(IsPayButtonEnabled());

    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

    ExpectBodyContains({"bobpay.test"});
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest,
                       ReadSupportedDelegationsFromAppManifest) {
  // Trigger a request that specifies kylepay.test and asks for shipping address
  // as well as payer's contact information. kylepay.test hosts an installable
  // payment app which handles both shipping address and payer's contact
  // information.
  NavigateTo("/payment_request_bobpay_and_cards_test.html");
  SetDownloaderAndIgnorePortInOriginComparisonForTesting();
  ResetEventWaiterForDialogOpened();
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.test/webpay'}], "
      "true /*= requestShippingContact */);",
      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());

  // Pay button should be enabled without any autofill profiles since the
  // selected payment instrument (kylepay) handles all merchant required
  // information.
  EXPECT_TRUE(IsPayButtonEnabled());

  ResetEventWaiterForSequence({DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains({"kylepay.test/webpay"});
}
}  // namespace payments
