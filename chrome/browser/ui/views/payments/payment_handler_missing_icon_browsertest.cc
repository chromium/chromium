// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "components/payments/core/features.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Test the UX flow for service worker payment apps that have missing icons.
//
// These apps are no longer allowed to be installed, but for any that were
// installed before that change, they should not skip the sheet.
class PaymentHandlerMissingIconTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentHandlerMissingIconTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAllowJITInstallationWhenAppIconIsMissing);
  }

  ~PaymentHandlerMissingIconTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Setup Kylepay server.
    kylepay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/kylepay.test/");
    ASSERT_TRUE(kylepay_server_.Start());
  }

  // Sets a TestDownloader for ServiceWorkerPaymentAppFinder and ignores port in
  // app scope. Must be called while on the page that will invoke the
  // PaymentRequest API, because ServiceWorkerPaymentAppFinder is owned by the
  // page.
  void SetDownloaderAndIgnorePortInOriginComparisonForTesting() {
    content::BrowserContext* context =
        GetActiveWebContents()->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        GetCSPCheckerForTests(), context->GetDefaultStoragePartition()
                                     ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://kylepay.test/",
                                 kylepay_server_.GetURL("kylepay.test", "/"));
    ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(
        GetActiveWebContents()->GetPrimaryMainFrame())
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));
  }

 private:
  net::EmbeddedTestServer kylepay_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerMissingIconTest, CantSkipTheSheet) {
  // Navigate to a page with strict CSP so that Kylepay's icon fetch fails.
  NavigateTo("/csp_prevent_icon_download.html");
  SetDownloaderAndIgnorePortInOriginComparisonForTesting();

  // Create a payment request for Kylepay.
  ResetEventWaiterForDialogOpened();
  content::ExecuteScriptAsync(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.test/webpay'}], "
      "/* requestShippingContact= */ true);");
  ASSERT_TRUE(WaitForObservedEvent());

  // App with missing icon is not preselectable.
  EXPECT_FALSE(IsPayButtonEnabled());

  // Open payment method section to explicitly choose Kylepay
  OpenPaymentMethodScreen();
  ResetEventWaiter(DialogEvent::BACK_NAVIGATION);
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  EXPECT_TRUE(list_view);
  EXPECT_EQ(1u, list_view->children().size());
  ClickOnDialogViewAndWait(list_view->children()[0]);
  // Pay button should be enabled now.
  EXPECT_TRUE(IsPayButtonEnabled());

  // Click on Pay to install Kylepay and complete the payment.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  ExpectBodyContains({"kylepay.test/webpay"});
}

}  // namespace payments
