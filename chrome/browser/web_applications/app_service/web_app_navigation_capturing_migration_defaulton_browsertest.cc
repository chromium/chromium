// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

using apps_util::PreferredAppsListReadyWaiter;

webapps::AppId FindAppIdByPath(WebAppProvider& provider,
                               std::string_view path) {
  for (const WebApp& app : provider.registrar_unsafe().GetApps()) {
    if (app.start_url().path() == path) {
      return app.app_id();
    }
  }
  ADD_FAILURE() << "App not found for path: " << path;
  return "";
}

// Verifies the automated startup migration routine for PWA navigation capturing
// user preferences across two successive browser sessions:
// 1. `PRE_*`: Establishes the initial baseline state under `default-off`
//    (`reimpl_default_off`). Installs test applications and marks a specific
//    app (App B) as preferred by the user.
// 2. Main test: Simulates an upgrade/restart where navigation capturing is
//    enabled (`GetParam()`, which is `default-on` or `on-via-client-mode`). The
//    browser automatically executes startup state migration, keeps App B
//    preferred, migrates valid applications to preferred, and stores a backup
//    of App B.
class WebAppNavigationCapturingMigrationSuccessTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<std::string> {
 public:
  WebAppNavigationCapturingMigrationSuccessTest() {
    std::string_view test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.starts_with("DISABLED_")) {
      test_name.remove_prefix(9);
    }

    std::string link_capturing_state = "reimpl_default_off";
    if (!test_name.starts_with("PRE_")) {
      link_capturing_state = GetParam();
    }

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing,
        {{"link_capturing_state", link_capturing_state}});
  }

  WebAppNavigationCapturingMigrationSuccessTest(
      const WebAppNavigationCapturingMigrationSuccessTest&) = delete;
  WebAppNavigationCapturingMigrationSuccessTest& operator=(
      const WebAppNavigationCapturingMigrationSuccessTest&) = delete;
  ~WebAppNavigationCapturingMigrationSuccessTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationSuccessTest,
                       PRE_MigrationOnStartup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install App A (standard web app, no client mode).
  const GURL url_a =
      embedded_test_server()->GetURL("/web_apps/site_d/basic.html");
  webapps::AppId app_a = InstallWebAppFromManifest(browser(), url_a);
  apps::AppReadinessWaiter(profile(), app_a).Await();

  // Install App B (standard web app, no client mode).
  const GURL url_b =
      embedded_test_server()->GetURL("/web_apps/standalone/basic.html");
  webapps::AppId app_b = InstallWebAppFromManifest(browser(), url_b);
  apps::AppReadinessWaiter(profile(), app_b).Await();

  // Mark App B as user preferred before migration.
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_b);

  // Install App C (has client mode: focus-existing).
  const GURL url_c = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");
  webapps::AppId app_c = InstallWebAppFromPage(browser(), url_c);
  apps::AppReadinessWaiter(profile(), app_c).Await();

  // Only app B should be set to capture links.
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  PrefService* prefs = profile()->GetPrefs();
  EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                prefs::kLastNavigationCapturingMigrationState)),
            MigrationState::kDefaultOff);
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationSuccessTest,
                       MigrationOnStartup) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  webapps::AppId app_a =
      FindAppIdByPath(provider(), "/web_apps/site_d/basic.html");
  webapps::AppId app_b =
      FindAppIdByPath(provider(), "/web_apps/standalone/basic.html");
  webapps::AppId app_c = FindAppIdByPath(provider(), "/web_apps/basic.html");

  PrefService* prefs = profile()->GetPrefs();

  // App A was migrated to preferred only if the flag state is `default-on`,
  // regardless of client_mode.
  if (GetParam() == "reimpl_default_on") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kDefaultOn);
    EXPECT_TRUE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  } else if (GetParam() == "reimpl_on_via_client_mode") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kOnViaClientMode);
    // App A is NOT preferred (standard web app, no client mode).
    EXPECT_FALSE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  }

  // App B remains preferred (user setting).
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));

  // App C is now preferred (migrated because it has client mode, or because
  // it's a standard app under DefaultOn).
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  // Backup contains App B.
  const base::ListValue& backup =
      prefs->GetList(prefs::kWebAppsPreviouslyAppSupportedLinks);
  EXPECT_EQ(backup.size(), 1u);
  EXPECT_EQ(backup[0].GetString(), app_b);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppNavigationCapturingMigrationSuccessTest,
                         testing::Values("reimpl_default_on",
                                         "reimpl_on_via_client_mode"),
                         [](const testing::TestParamInfo<std::string>& info) {
                           return info.param == "reimpl_default_on"
                                      ? "DefaultOn"
                                      : "OnViaClientMode";
                         });

// Verifies the automated startup rollback routine for PWA navigation capturing
// user preferences across three successive browser sessions:
// 1. `PRE_PRE_*`: Establishes the initial baseline state under `default-off`
//    (`reimpl_default_off`). Installs test applications and sets a specific
//    app (App B) as preferred by the user.
// 2. `PRE_*`: Simulates an upgrade/restart where navigation capturing is
//    enabled (`GetParam()`, which is `default-on` or `on-via-client-mode`). The
//    browser automatically runs startup migration, makes valid applications
//    preferred, and stores a backup of the user's original preferred app
//    (App B).
// 3. Main test: Simulates a rollback where navigation capturing is
//    turned off again (`reimpl_default_off`). The browser detects the rollback,
//    restores the original preferred app (App B) from the backup preference,
//    and un-prefers any applications that were automatically migrated.
class WebAppNavigationCapturingMigrationRollbackTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<std::string> {
 public:
  WebAppNavigationCapturingMigrationRollbackTest() {
    std::string_view test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.starts_with("DISABLED_")) {
      test_name.remove_prefix(9);
    }

    std::string link_capturing_state = "reimpl_default_off";
    if (test_name.starts_with("PRE_") && !test_name.starts_with("PRE_PRE_")) {
      link_capturing_state = GetParam();
    }

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing,
        {{"link_capturing_state", link_capturing_state}});
  }

  WebAppNavigationCapturingMigrationRollbackTest(
      const WebAppNavigationCapturingMigrationRollbackTest&) = delete;
  WebAppNavigationCapturingMigrationRollbackTest& operator=(
      const WebAppNavigationCapturingMigrationRollbackTest&) = delete;
  ~WebAppNavigationCapturingMigrationRollbackTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationRollbackTest,
                       PRE_PRE_RollbackOnStartup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install App A.
  const GURL url_a =
      embedded_test_server()->GetURL("/web_apps/site_d/basic.html");
  webapps::AppId app_a = InstallWebAppFromManifest(browser(), url_a);
  apps::AppReadinessWaiter(profile(), app_a).Await();

  // Install App B.
  const GURL url_b =
      embedded_test_server()->GetURL("/web_apps/standalone/basic.html");
  webapps::AppId app_b = InstallWebAppFromManifest(browser(), url_b);
  apps::AppReadinessWaiter(profile(), app_b).Await();

  // Install App C.
  const GURL url_c = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");
  webapps::AppId app_c = InstallWebAppFromPage(browser(), url_c);
  apps::AppReadinessWaiter(profile(), app_c).Await();

  // Mark App B as user preferred before migration.
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_b);

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  PrefService* prefs = profile()->GetPrefs();
  EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                prefs::kLastNavigationCapturingMigrationState)),
            MigrationState::kDefaultOff);
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationRollbackTest,
                       PRE_RollbackOnStartup) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  webapps::AppId app_a =
      FindAppIdByPath(provider(), "/web_apps/site_d/basic.html");
  webapps::AppId app_b =
      FindAppIdByPath(provider(), "/web_apps/standalone/basic.html");
  webapps::AppId app_c = FindAppIdByPath(provider(), "/web_apps/basic.html");

  PrefService* prefs = profile()->GetPrefs();
  if (GetParam() == "reimpl_default_on") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kDefaultOn);
    EXPECT_TRUE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  } else if (GetParam() == "reimpl_on_via_client_mode") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kOnViaClientMode);
    EXPECT_FALSE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  }

  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  // Backup contains App B.
  const base::ListValue& backup =
      prefs->GetList(prefs::kWebAppsPreviouslyAppSupportedLinks);
  EXPECT_EQ(backup.size(), 1u);
  EXPECT_EQ(backup[0].GetString(), app_b);
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationRollbackTest,
                       RollbackOnStartup) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  webapps::AppId app_a =
      FindAppIdByPath(provider(), "/web_apps/site_d/basic.html");
  webapps::AppId app_b =
      FindAppIdByPath(provider(), "/web_apps/standalone/basic.html");
  webapps::AppId app_c = FindAppIdByPath(provider(), "/web_apps/basic.html");

  // Verify that the rollback ran automatically on startup!
  PrefService* prefs = profile()->GetPrefs();
  EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                prefs::kLastNavigationCapturingMigrationState)),
            MigrationState::kDefaultOff);

  // App A and App C are no longer preferred (rolled back).
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  // App B remains preferred (restored).
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));

  // Backup cleared.
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kWebAppsPreviouslyAppSupportedLinks));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppNavigationCapturingMigrationRollbackTest,
                         testing::Values("reimpl_default_on",
                                         "reimpl_on_via_client_mode"),
                         [](const testing::TestParamInfo<std::string>& info) {
                           return info.param == "reimpl_default_on"
                                      ? "DefaultOn"
                                      : "OnViaClientMode";
                         });

#if BUILDFLAG(IS_CHROMEOS)
// Verifies that migration is blocked if a conflicting System Web App is already
// preferred for the same scope.
class WebAppNavigationCapturingMigrationSwaConflictTest
    : public WebAppBrowserTestBase {
 public:
  WebAppNavigationCapturingMigrationSwaConflictTest() {
    std::string_view test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.starts_with("DISABLED_")) {
      test_name.remove_prefix(9);
    }

    std::string link_capturing_state = "reimpl_default_off";
    if (!test_name.starts_with("PRE_")) {
      link_capturing_state = "reimpl_default_on";
    }

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing,
        {{"link_capturing_state", link_capturing_state}});
  }

  WebAppNavigationCapturingMigrationSwaConflictTest(
      const WebAppNavigationCapturingMigrationSwaConflictTest&) = delete;
  WebAppNavigationCapturingMigrationSwaConflictTest& operator=(
      const WebAppNavigationCapturingMigrationSwaConflictTest&) = delete;
  ~WebAppNavigationCapturingMigrationSwaConflictTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingMigrationSwaConflictTest,
                       PRE_SwaConflictOnStartup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install App A (standard web app).
  GURL scope("https://example.com/site/");
  GURL start_url_a("https://example.com/site/index_a.html");
  GURL start_url_e("https://example.com/site/index_e.html");

  auto info_a = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(start_url_a), start_url_a);
  info_a->scope = scope;
  info_a->title = u"Web App A";
  info_a->user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_a = test::InstallWebApp(profile(), std::move(info_a));
  apps::AppReadinessWaiter(profile(), app_a).Await();

  // Install App E (System Web App) using SYSTEM_DEFAULT.
  auto info_e = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(start_url_e), start_url_e);
  info_e->scope = scope;
  info_e->title = u"SWA App E";
  info_e->user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_e =
      test::InstallWebApp(profile(), std::move(info_e),
                          /*overwrite_existing_manifest_fields=*/true,
                          webapps::WebappInstallSource::SYSTEM_DEFAULT);
  apps::AppReadinessWaiter(profile(), app_e).Await();
  EXPECT_TRUE(provider().registrar_unsafe().IsSystemApp(app_e));

  // Mark SWA E as preferred (capturing the link). This overrides any other
  // preference.
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_e);

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_e));
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingMigrationSwaConflictTest,
                       SwaConflictOnStartup) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  GURL start_url_a("https://example.com/site/index_a.html");
  webapps::AppId app_a = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url_a));

  // App A must NOT be migrated to preferred, because SWA E is currently set as
  // preferred for the conflicting scope.
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));

  GURL start_url_e("https://example.com/site/index_e.html");
  webapps::AppId app_e = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url_e));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_e));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
