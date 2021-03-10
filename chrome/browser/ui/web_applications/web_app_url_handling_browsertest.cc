// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace {

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";

blink::mojom::WebFeature url_handling_feature =
    blink::mojom::WebFeature::kWebAppManifestUrlHandlers;

}  // namespace

namespace web_app {

class WebAppUrlHandlingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppUrlHandlingBrowserTest() {
    os_hooks_supress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableUrlHandlers);
  }

  ~WebAppUrlHandlingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  AppId InstallTestApp(const char* path, bool await_metric) {
    GURL start_url = embedded_test_server()->GetURL(path);
    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric)
      metrics_waiter.AddWebFeatureExpectation(url_handling_feature);

    AppId app_id = web_app::InstallWebAppFromPage(browser(), start_url);
    if (await_metric)
      metrics_waiter.Wait();

    return app_id;
  }

  WebAppProviderBase& provider() {
    auto* provider = WebAppProviderBase::GetProviderBase(browser()->profile());
    DCHECK(provider);
    return *provider;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  ScopedOsHooksSuppress os_hooks_supress_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlingBrowserTest, BasicUrlHandlers) {
  AppId app_id = InstallTestApp(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_url_handlers.json",
      /*await_metric=*/true);
  apps::UrlHandlers url_handlers =
      provider().registrar().GetAppUrlHandlers(app_id);

  // One handler has an invalid host so it shouldn't be in the result.
  ASSERT_EQ(3u, url_handlers.size());
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://test.com")),
                           /*has_origin_wildcard=*/false)));
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://example.com")),
                           /*has_origin_wildcard=*/false)));
  EXPECT_TRUE(base::Contains(
      url_handlers,
      apps::UrlHandlerInfo(url::Origin::Create(GURL("https://example.com")),
                           /*has_origin_wildcard=*/true)));

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      url_handling_feature, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlingBrowserTest, NoUrlHandlers) {
  AppId app_id =
      InstallTestApp("/banners/manifest_test_page.html?manifest=manifest.json",
                     /*await_metric=*/false);
  apps::UrlHandlers url_handlers =
      provider().registrar().GetAppUrlHandlers(app_id);
  ASSERT_EQ(0u, url_handlers.size());

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      url_handling_feature, 0);
}

}  // namespace web_app
