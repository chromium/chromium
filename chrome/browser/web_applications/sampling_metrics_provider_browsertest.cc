// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/sampling_metrics_provider.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/widget_test_api.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace web_app {
namespace {
class WebAppSamplingMetricsProviderBrowserTest : public WebAppBrowserTestBase {
 public:
  static constexpr std::string_view kMigrateFromSuggestUrl =
      "/web_apps/migration/migrate_from/suggest.html";
  static constexpr std::string_view kMigrateToSuggestUrl =
      "/web_apps/migration/migrate_to/suggest.html";

  WebAppSamplingMetricsProviderBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppMigrationApi);
  }

  void EmitMetrics() {
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          SamplingMetricsProvider::EmitMetrics();
          run_loop.Quit();
        }));
    run_loop.Run();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// `Measure()` should not cause a crash when called between the close request
// and the window being closed. See b/378020140 for more details.
IN_PROC_BROWSER_TEST_F(WebAppSamplingMetricsProviderBrowserTest,
                       NoCrashOnClose) {
  // Install and launch an app browser.
  webapps::AppId app_id = InstallPWA(GetInstallableAppURL());
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);

  bool measure_called = false;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](bool* called) {
                       SamplingMetricsProvider::EmitMetrics();
                       *called = true;
                     },
                     &measure_called));

  content::WebContentsDestroyedWatcher destroyed_watcher(
      app_browser->tab_strip_model()->GetWebContentsAt(0));
  ui_test_utils::BrowserDestroyedObserver observer(app_browser);
  app_browser->tab_strip_model()->CloseAllTabs();
  destroyed_watcher.Wait();

  observer.Wait();

  EXPECT_TRUE(measure_called);
}

IN_PROC_BROWSER_TEST_F(WebAppSamplingMetricsProviderBrowserTest, NoInstalls) {
  base::HistogramTester histograms;
  EmitMetrics();
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationSuggested", false, 1);
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationDialogShowing", false, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppSamplingMetricsProviderBrowserTest, NoMigration) {
  InstallPWA(GetInstallableAppURL());
  base::HistogramTester histograms;
  EmitMetrics();
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationSuggested", false, 1);
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationDialogShowing", false, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppSamplingMetricsProviderBrowserTest,
                       HasMigrationAvailable) {
  // Install app A (from).
  webapps::AppId app_a_id = InstallWebAppFromPage(
      browser(), embedded_https_test_server().GetURL(kMigrateFromSuggestUrl));

  // Install app B (to).
  InstallWebAppFromPage(
      browser(), embedded_https_test_server().GetURL(kMigrateToSuggestUrl));

  // Launch app A in a standalone window.
  LaunchWebAppBrowser(app_a_id);

  base::HistogramTester histograms;
  EmitMetrics();
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationSuggested", true, 1);
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationDialogShowing", false, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppSamplingMetricsProviderBrowserTest,
                       MigrationDialogShowing) {
  // Install app A (from).
  webapps::AppId app_a_id = InstallWebAppFromPage(
      browser(), embedded_https_test_server().GetURL(kMigrateFromSuggestUrl));

  // Install app B (to).
  InstallWebAppFromPage(
      browser(), embedded_https_test_server().GetURL(kMigrateToSuggestUrl));

  // Launch app A in a standalone window.
  Browser* app_a_browser = LaunchWebAppBrowser(app_a_id);

  // Trigger the migration dialog.
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");

  WebAppBrowserController* controller =
      WebAppBrowserController::From(app_a_browser);
  ASSERT_TRUE(controller);
  controller->TriggerAppUpdateOrMigrationDialog(base::TimeTicks::Now());

  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget != nullptr);

  base::HistogramTester histograms;
  EmitMetrics();
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationSuggested", true, 1);
  histograms.ExpectBucketCount(
      "WebApp.Engagement2.Standalone.MigrationDialogShowing", true, 1);
}

}  // namespace
}  // namespace web_app
