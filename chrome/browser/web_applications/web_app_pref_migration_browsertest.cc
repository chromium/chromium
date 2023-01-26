// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include <string>

#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const auto kExpectedInstallSource =
    webapps::WebappInstallSource::MANAGEMENT_API;

}  // namespace

class WebAppPrefMigrationBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppPrefMigrationBrowserTest() = default;
  ~WebAppPrefMigrationBrowserTest() override = default;

  WebAppRegistrar& registrar() {
    return WebAppProvider::GetForTest(browser()->profile())->registrar_unsafe();
  }

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }
};

IN_PROC_BROWSER_TEST_F(WebAppPrefMigrationBrowserTest, PRE_Migration) {
  AppId app_id = test::InstallDummyWebApp(browser()->profile(), "Test app 1",
                                          GURL("https://example.com/app_1"),
                                          webapps::WebappInstallSource::ARC);
  // New installs should no longer write into prefs.
  EXPECT_FALSE(GetWebAppInstallSourceDeprecated(
      browser()->profile()->GetPrefs(), app_id));

  // Set the prefstore and database into a state that simulates pre-migration.
  // (Installation above will use the new code path and not require migration.)
  {
    UpdateIntWebAppPref(prefs(), app_id, "latest_web_app_install_source",
                        static_cast<int>(kExpectedInstallSource));
    ScopedRegistryUpdate update(
        &WebAppProvider::GetForTest(browser()->profile())
             ->sync_bridge_unsafe());
    WebApp* web_app = update->UpdateApp(app_id);
    web_app->SetInstallSourceForMetrics(absl::nullopt);
  }

  absl::optional<int> install_source =
      GetWebAppInstallSourceDeprecated(prefs(), app_id);
  ASSERT_TRUE(install_source);
  EXPECT_EQ(static_cast<int>(kExpectedInstallSource), *install_source);

  EXPECT_EQ(kExpectedInstallSource,
            *registrar().GetAppInstallSourceForMetrics(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppPrefMigrationBrowserTest, Migration) {
  AppId app_id = registrar().GetAppIds()[0];
  absl::optional<int> install_source = GetWebAppInstallSourceDeprecated(
      browser()->profile()->GetPrefs(), app_id);
  ASSERT_FALSE(install_source);

  EXPECT_EQ(kExpectedInstallSource,
            *registrar().GetAppInstallSourceForMetrics(app_id));
}

}  // namespace web_app
