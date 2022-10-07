// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace web_app {

// TODO(https://crbug.com/1353659): Enable tests on Lacros.
// This feature depends on
// https://chromium-review.googlesource.com/c/chromium/src/+/3867152 landing
// to be able to work in Lacros. Currently Lacros doesn't know when the web app
// publisher has been initialised.

#if !BUILDFLAG(IS_CHROMEOS_LACROS)

class WebAppIconHealthChecksBrowserTest : public InProcessBrowserTest {
 public:
  WebAppIconHealthChecksBrowserTest() {
    WebAppMetrics::DisableAutomaticIconHealthChecksForTesting();
  }

  ~WebAppIconHealthChecksBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* profile() { return browser()->profile(); }

  ScopedRegistryUpdate CreateUpdateScope() {
    return ScopedRegistryUpdate(
        &WebAppProvider::GetForTest(profile())->sync_bridge());
  }

  void RunIconChecksWithMetricExpectations(
      WebAppIconDiagnostic::Result expected_result) {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    WebAppMetrics::Get(profile())->icon_health_checks_for_testing().Start(
        run_loop.QuitClosure());
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "WebApp.Icon.AppsWithEmptyDownloadedIconSizes",
        expected_result.has_empty_downloaded_icon_sizes, 1);
    histogram_tester.ExpectUniqueSample("WebApp.Icon.AppsWithGeneratedIconFlag",
                                        expected_result.has_generated_icon_flag,
                                        1);
  }

  AppId InstallWebAppAndAwaitAppService(const char* path) {
    AppId app_id =
        InstallWebAppFromPage(browser(), embedded_test_server()->GetURL(path));
    AppReadinessWaiter(profile(), app_id, apps::Readiness::kReady).Await();
    return app_id;
  }

  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
};

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, HealthyIcons) {
  InstallWebAppAndAwaitAppService("/web_apps/basic.html");
  RunIconChecksWithMetricExpectations({});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest,
                       MissingDownloadedIconSizes) {
  AppId app_id = InstallWebAppAndAwaitAppService("/web_apps/basic.html");
  CreateUpdateScope()->UpdateApp(app_id)->SetDownloadedIconSizes(
      IconPurpose::ANY, {});
  RunIconChecksWithMetricExpectations(
      {.has_empty_downloaded_icon_sizes = true});
}

IN_PROC_BROWSER_TEST_F(WebAppIconHealthChecksBrowserTest, GeneratedIcon) {
  InstallWebAppAndAwaitAppService("/web_apps/get_manifest.html?no_icons.json");
  RunIconChecksWithMetricExpectations({.has_generated_icon_flag = true});
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace web_app
