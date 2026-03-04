// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {
namespace {

class InstallMigrateToAppCommandBrowserTest : public WebAppBrowserTestBase {
 public:
  static constexpr std::string_view kMigrateFromSuggestUrl =
      "/web_apps/migration/migrate_from/suggest.html";
  static constexpr std::string_view kMigrateToManifestId =
      "/web_apps/migration/migrate_to/manifest_id";

  InstallMigrateToAppCommandBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebAppMigrationApi,
         features::kWebAppPredictableAppUpdating},
        {});
  }

  webapps::AppId GetTargetAppId() {
    return GenerateAppIdFromManifestId(
        webapps::ManifestId(https_server()->GetURL(kMigrateToManifestId)));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InstallMigrateToAppCommandBrowserTest,
                       NotScheduledWhenSourceNotInstalled) {
  // 1. Navigate to source app's page without installing it.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kMigrateFromSuggestUrl)));
  test::WaitForLoadCompleteAndMaybeManifestSeen(
      *browser()->tab_strip_model()->GetActiveWebContents());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // 2. Verify target app is NOT installed.
  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetInstallState(GetTargetAppId())
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(InstallMigrateToAppCommandBrowserTest,
                       TargetInstalledWhenSourceIsInstalled) {
  // 1. Install the source app. Since it's from `migrate_from/suggest.html`,
  // this will install the app and its manifest will have `migrate_to`.
  InstallWebAppFromPage(browser(),
                        https_server()->GetURL(kMigrateFromSuggestUrl));

  base::RunLoop run_loop;
  WebAppInstallManagerObserverAdapter observer(&provider().install_manager());
  observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id) {
        if (installed_app_id == GetTargetAppId()) {
          run_loop.Quit();
        }
      }));

  // 2. Navigate to trigger ManifestUpdateManager.
  // We navigate again to the source app to re-trigger manifest check, which
  // schedules the migration to the target app.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kMigrateFromSuggestUrl)));
  test::WaitForLoadCompleteAndMaybeManifestSeen(
      *browser()->tab_strip_model()->GetActiveWebContents());

  // Wait for the installation of the target app.
  run_loop.Run();

  // 3. Verify the target app is installed.
  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetInstallState(GetTargetAppId())
                  .has_value());
}

}  // namespace
}  // namespace web_app
