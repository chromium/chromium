// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/simple_web_view_dialog.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace {

using security_interstitials::https_only_mode::Event;
using security_interstitials::https_only_mode::kEventHistogram;

// Url used to detect the presence of a captive portal.
constexpr char kCaptivePortalPingUrl[] = "http://captive-portal-ping-url.com/";
// HTTPS version of the same URL.
constexpr char kCaptivePortalPingUrlHttps[] =
    "https://captive-portal-ping-url.com/";

}  // namespace

namespace ash {

using SimpleWebViewDialogTest = ::InProcessBrowserTest;

// Tests that http auth triggered web dialog does not crash.
IN_PROC_BROWSER_TEST_F(SimpleWebViewDialogTest, HttpAuthWebDialog) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto dialog_ptr = std::make_unique<SimpleWebViewDialog>(browser()->profile());
  auto delegate = dialog_ptr->MakeWidgetDelegate();
  auto* dialog = delegate->SetContentsView(std::move(dialog_ptr));

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.delegate = delegate.release();  // Pass ownership to widget.

  views::UniqueWidgetPtr widget(std::make_unique<ChromeTestWidget>());
  widget->Init(std::move(params));

  // Load a http auth challenged page.
  dialog->StartLoad(embedded_test_server()->GetURL("/auth-basic"));
  dialog->Init();

  // Wait for http auth login view to show up. No crash should happen.
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));
}

// Returns a URL loader interceptor that returns an HTTP 200 response for all
// URLs.
std::unique_ptr<content::URLLoaderInterceptor> MakeCaptivePortalInterceptor() {
  return std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url == GURL(kCaptivePortalPingUrl)) {
              // Serve a link to the http URL. Clicking it in the test should
              // navigate to the https URL.
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  base::StringPrintf(
                      "<html><a href='%s' id=login>Login</a></html>",
                      kCaptivePortalPingUrl),
                  params->client.get());
              return true;
            }
            if (params->url_request.url == GURL(kCaptivePortalPingUrlHttps)) {
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  "<html>Done</html>", params->client.get());
              return true;
            }
            return false;
          }));
}

IN_PROC_BROWSER_TEST_F(SimpleWebViewDialogTest, NoHttpsUpgradeOnInitialLoad) {
  auto interceptor = MakeCaptivePortalInterceptor();

  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);

  auto dialog_ptr = std::make_unique<SimpleWebViewDialog>(browser()->profile());
  auto delegate = dialog_ptr->MakeWidgetDelegate();
  auto* dialog = delegate->SetContentsView(std::move(dialog_ptr));

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.delegate = delegate.release();  // Pass ownership to widget.

  views::UniqueWidgetPtr widget(std::make_unique<ChromeTestWidget>());
  widget->Init(std::move(params));

  base::HistogramTester histograms;
  // Load an HTTP URL. It shouldn't be upgraded to HTTPS.
  dialog->StartLoad(GURL(kCaptivePortalPingUrl));
  dialog->Init();
  histograms.ExpectTotalCount(kEventHistogram, 0);

  content::WebContents* contents = dialog->GetActiveWebContents();
  content::WaitForLoadStop(contents);
  EXPECT_EQ(kCaptivePortalPingUrl, contents->GetLastCommittedURL());

  // Subsequent navigations should get upgraded.
  content::TestNavigationObserver observer(contents, 1);
  ASSERT_TRUE(
      content::ExecJs(contents, "document.getElementById('login').click()"));
  observer.Wait();
  EXPECT_EQ(kCaptivePortalPingUrlHttps, contents->GetLastCommittedURL());

  histograms.ExpectTotalCount(kEventHistogram, 2);
  histograms.ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms.ExpectBucketCount(kEventHistogram, Event::kUpgradeSucceeded, 1);
}

}  // namespace ash
