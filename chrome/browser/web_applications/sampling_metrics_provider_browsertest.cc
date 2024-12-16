// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/sampling_metrics_provider.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace web_app {
namespace {
using SamplingMetricsProviderBrowserTest = WebAppBrowserTestBase;

// `Measure()` should not cause a crash when called between the cloase request
// and the window being closed. See b/378020140 for more details.
IN_PROC_BROWSER_TEST_F(SamplingMetricsProviderBrowserTest, NoCrashOnClose) {
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
  app_browser->tab_strip_model()->CloseAllTabs();
  destroyed_watcher.Wait();

  ui_test_utils::WaitForBrowserToClose(app_browser);

  EXPECT_TRUE(measure_called);
}

}  // namespace
}  // namespace web_app
