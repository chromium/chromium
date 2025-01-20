// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class ShortcutToDiyAppMigrationBrowserTest : public InProcessBrowserTest {
 public:
  ShortcutToDiyAppMigrationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kMigrateShortcutsToDiy);
  }
  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Installs a shortcut app in the profile by emptying its scope.
IN_PROC_BROWSER_TEST_F(ShortcutToDiyAppMigrationBrowserTest,
                       PRE_MigrateShortcutToDiyApp) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/"));
  web_app_info->scope = GURL();

  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_info),
                          /*overwrite_existing_manifest_fields=*/false,
                          webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

  auto* provider = WebAppProvider::GetForWebApps(profile());
  ASSERT_TRUE(
      provider->registrar_unsafe().GetAppById(app_id)->scope().is_empty());
}

// Verifies that on next startup of the WebAppProvider, the installed shortcut
// app is migrated to DIY app.
IN_PROC_BROWSER_TEST_F(ShortcutToDiyAppMigrationBrowserTest,
                       MigrateShortcutToDiyApp) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  webapps::AppId app_id = provider->registrar_unsafe().GetAppIds().at(0);
  ASSERT_TRUE(provider->registrar_unsafe().GetAppById(app_id)->is_diy_app());
}

}  // namespace
}  // namespace web_app
