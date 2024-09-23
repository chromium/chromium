// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";

blink::mojom::WebFeature window_controls_overlay_feature =
    blink::mojom::WebFeature::kWebAppWindowControlsOverlay;

}  // namespace

namespace web_app {

class WebAppWindowControlsOverlayBrowserTest
    : public WebAppNavigationBrowserTest {
 public:
  WebAppWindowControlsOverlayBrowserTest() = default;
  ~WebAppWindowControlsOverlayBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  webapps::AppId InstallTestApp(const char* path, bool await_metric) {
    GURL start_url = embedded_test_server()->GetURL(path);
    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric)
      metrics_waiter.AddWebFeatureExpectation(window_controls_overlay_feature);

    webapps::AppId app_id =
        web_app::InstallWebAppFromPage(browser(), start_url);
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
};

IN_PROC_BROWSER_TEST_F(WebAppWindowControlsOverlayBrowserTest,
                       BasicDisplayOverride) {
  webapps::AppId app_id = InstallTestApp(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_window_controls_overlay.json",
      /*await_metric=*/true);

  std::vector<DisplayMode> display_mode_override =
      provider().registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(1u, display_mode_override.size());

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      window_controls_overlay_feature, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppWindowControlsOverlayBrowserTest,
                       NoDisplayOverride) {
  webapps::AppId app_id =
      InstallTestApp("/banners/manifest_test_page.html?manifest=manifest.json",
                     /*await_metric=*/false);

  std::vector<DisplayMode> display_mode_override =
      provider().registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(0u, display_mode_override.size());

  histogram_tester_.ExpectBucketCount(kUseCounterHistogram,
                                      window_controls_overlay_feature, 0);
}

}  // namespace web_app
