// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/handle_links.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace web_app {

namespace {

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";

const blink::mojom::WebFeature handle_links_feature =
    blink::mojom::WebFeature::kWebAppManifestHandleLinks;

}  // namespace

class WebAppHandleLinksBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppHandleLinksBrowserTest() = default;

  ~WebAppHandleLinksBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  AppId InstallTestApp(const char* path, bool await_metric) {
    GURL start_url = embedded_test_server()->GetURL(path);
    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric)
      metrics_waiter.AddWebFeatureExpectation(handle_links_feature);

    AppId app_id = web_app::InstallWebAppFromPage(browser(), start_url);
    if (await_metric)
      metrics_waiter.Wait();

    return app_id;
  }

  WebAppProvider& provider() {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider);
    return *provider;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppEnableHandleLinks};
};

IN_PROC_BROWSER_TEST_F(WebAppHandleLinksBrowserTest, NoValue) {
  AppId app_id =
      InstallTestApp("/banners/manifest_test_page.html?manifest=manifest.json",
                     /*await_metric=*/true);

  blink::mojom::HandleLinks handle_links =
      provider().registrar().GetAppHandleLinks(app_id);
  ASSERT_EQ(handle_links, blink::mojom::HandleLinks::kAuto);

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      handle_links_feature, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppHandleLinksBrowserTest, StringValue) {
  AppId app_id = InstallTestApp(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_handle_links.json",
      /*await_metric=*/true);

  blink::mojom::HandleLinks handle_links =
      provider().registrar().GetAppHandleLinks(app_id);
  ASSERT_EQ(handle_links, blink::mojom::HandleLinks::kPreferred);

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      handle_links_feature, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppHandleLinksBrowserTest, ListValue) {
  AppId app_id = InstallTestApp(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_handle_links_list.json",
      /*await_metric=*/true);

  blink::mojom::HandleLinks handle_links =
      provider().registrar().GetAppHandleLinks(app_id);
  ASSERT_EQ(handle_links, blink::mojom::HandleLinks::kPreferred);

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      handle_links_feature, 1);
}

}  // namespace web_app
