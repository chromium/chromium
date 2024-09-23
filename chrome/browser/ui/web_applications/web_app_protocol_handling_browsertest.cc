// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";

blink::mojom::WebFeature protocol_handling_feature =
    blink::mojom::WebFeature::kWebAppManifestProtocolHandlers;

}  // namespace

namespace web_app {

class WebAppProtocolHandlingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppProtocolHandlingBrowserTest() = default;
  ~WebAppProtocolHandlingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  webapps::AppId InstallTestApp(const char* path, bool await_metric) {
    GURL start_url = embedded_test_server()->GetURL(path);
    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric)
      metrics_waiter.AddWebFeatureExpectation(protocol_handling_feature);

    webapps::AppId app_id =
        web_app::InstallWebAppFromPage(browser(), start_url);
    if (await_metric)
      metrics_waiter.Wait();

    return app_id;
  }

  web_app::WebAppProvider* provider() {
    return WebAppProvider::GetForTest(browser()->profile());
  }

  web_app::WebAppProtocolHandlerManager& protocol_handler_manager() {
    return provider()
        ->os_integration_manager()
        .protocol_handler_manager_for_testing();
  }

 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(WebAppProtocolHandlingBrowserTest,
                       BasicProtocolHandlers) {
  webapps::AppId app_id = InstallTestApp(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_protocol_handlers.json",
      /*await_metric=*/true);
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers =
      protocol_handler_manager().GetAppProtocolHandlerInfos(app_id);

  // Two handlers have invalid properties so they shouldn't be in the result.
  apps::ProtocolHandlerInfo protocol_handler1;
  protocol_handler1.protocol = "mailto";
  protocol_handler1.url = GURL(embedded_test_server()->GetURL(
      "/banners/manifest_protocol_handlers.json?mailto=%s"));

  apps::ProtocolHandlerInfo protocol_handler2;
  protocol_handler2.protocol = "web+testing";
  protocol_handler2.url = GURL(embedded_test_server()->GetURL(
      "/banners/manifest_protocol_handlers.json?testing=%s"));

  ASSERT_EQ(2u, protocol_handlers.size());
  EXPECT_TRUE(base::Contains(protocol_handlers, protocol_handler1));
  EXPECT_TRUE(base::Contains(protocol_handlers, protocol_handler2));

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      protocol_handling_feature, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppProtocolHandlingBrowserTest, NoProtocolHandlers) {
  webapps::AppId app_id =
      InstallTestApp("/banners/manifest_test_page.html?manifest=manifest.json",
                     /*await_metric=*/false);
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers =
      protocol_handler_manager().GetAppProtocolHandlerInfos(app_id);
  ASSERT_EQ(0u, protocol_handlers.size());

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      protocol_handling_feature, 0);
}

}  // namespace web_app
