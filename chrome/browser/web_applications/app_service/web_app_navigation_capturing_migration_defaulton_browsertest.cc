// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_user_choice.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
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

namespace {

constexpr char kAppAStartUrl[] = "https://app-a.example/app/index.html";
constexpr char kAppBStartUrl[] = "https://app-b.example/app/index.html";
constexpr char kAppCStartUrl[] = "https://app-c.example/app/index.html";
constexpr char kAppEStartUrl[] = "https://app-e.example/app/index.html";
// In the server-side `user_settings_forced_off_apps` list.
constexpr char kAppFStartUrl[] = "https://app-f.example/app/index.html";
constexpr char kAppGStartUrl[] = "https://app-g.example/app/index.html";
constexpr char kAppHStartUrl[] = "https://app-h.example/app/index.html";
constexpr char kAppXStartUrl[] = "https://app-x.example/app/index.html";
// Hosts of pairs of web apps with the same scope. See `SameScopePair`.
constexpr char kSameScopeOptedInFirstHost[] = "same-scope-1.example";
constexpr char kSameScopeOptedInSecondHost[] = "same-scope-2.example";
constexpr char kSameScopeReplacedHost[] = "same-scope-3.example";
// An app that is not a web app (for example, an ARC app). It exists only as an
// entry in the PreferredAppsList.
constexpr char kNonWebAppId[] = "non-web-app-for-link-capturing-test";

int CurrentTestPreCount() {
  std::string_view test_name =
      testing::UnitTest::GetInstance()->current_test_info()->name();
  if (test_name.starts_with("DISABLED_")) {
    test_name.remove_prefix(9);
  }
  int count = 0;
  while (test_name.starts_with("PRE_")) {
    test_name.remove_prefix(4);
    ++count;
  }
  return count;
}

// `forced_off_app_id`, if not empty, is put in the server-side
// `user_settings_forced_off_apps` list of `kPwaNavigationCapturing`.
void InitMigrationFeatures(base::test::ScopedFeatureList& feature_list,
                           std::string_view link_capturing_state,
                           bool respect_user_choice,
                           std::string_view forced_off_app_id = {}) {
  base::FieldTrialParams params = {
      {"link_capturing_state", std::string(link_capturing_state)}};
  if (!forced_off_app_id.empty()) {
    params[features::kForcedOffCapturingAppsUserSetting.name] =
        std::string(forced_off_app_id);
  }
  std::vector<base::test::FeatureRefAndParams> enabled = {
      {features::kPwaNavigationCapturing, params}};
  std::vector<base::test::FeatureRef> disabled;
  if (respect_user_choice) {
    enabled.push_back(
        {apps::features::kRespectUserLinkCapturingChoiceOnChromeOS, {}});
  } else {
    disabled.push_back(
        apps::features::kRespectUserLinkCapturingChoiceOnChromeOS);
  }
  feature_list.InitWithFeaturesAndParameters(enabled, disabled);
}

webapps::AppId AppIdForStartUrl(const GURL& start_url) {
  return GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url));
}

webapps::AppId InstallAppWithOwnScope(Profile* profile, const GURL& start_url) {
  auto info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(start_url), start_url);
  info->scope = start_url.GetWithoutFilename();
  info->title = u"Test App";
  info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_id = test::InstallWebApp(profile, std::move(info));
  apps::AppReadinessWaiter(profile, app_id).Await();
  return app_id;
}

bool IsPreferred(Profile* profile, const webapps::AppId& app_id) {
  return apps::AppServiceProxyFactory::GetForProfile(profile)
      ->PreferredAppsList()
      .IsPreferredAppForSupportedLinks(app_id);
}

// Does the same as the ChromeOS Settings "Opening supported links" choice in
// `AppManagementPageHandlerChromeOs::SetPreferredApp()`, then waits until the
// web app commands write the recorded choices. The preferred apps list must be
// ready, so that the App Service change is synchronous.
void SimulateSettingsChoice(Profile* profile,
                            WebAppProvider& provider,
                            const webapps::AppId& app_id,
                            bool open_in_app) {
  apps::SetSupportedLinksPreferenceByUser(profile, app_id, open_in_app);
  provider.command_manager().AwaitAllCommandsCompleteForTesting();
}

// The migration runs from a posted task, so wait for its result.
[[nodiscard]] bool WaitForMigrationState(Profile* profile,
                                         MigrationState state) {
  return base::test::RunUntil([&]() {
    return static_cast<MigrationState>(profile->GetPrefs()->GetInteger(
               prefs::kLastNavigationCapturingMigrationState)) == state;
  });
}

void WaitForPreferredAppsList(Profile* profile) {
  PreferredAppsListReadyWaiter(
      apps::AppServiceProxyFactory::GetForProfile(profile)->PreferredAppsList())
      .Wait();
}

std::optional<std::string> PreferredAppForUrl(Profile* profile,
                                              const GURL& url) {
  return apps::AppServiceProxyFactory::GetForProfile(profile)
      ->PreferredAppsList()
      .FindPreferredAppForUrl(url);
}

base::flat_set<std::string> GetBackupAppIds(Profile* profile) {
  base::flat_set<std::string> app_ids;
  for (const base::Value& value : profile->GetPrefs()->GetList(
           prefs::kWebAppsPreviouslyAppSupportedLinks)) {
    if (const std::string* app_id = value.GetIfString()) {
      app_ids.insert(*app_id);
    }
  }
  return app_ids;
}

// Two web apps with the same scope (https://<host>/app/). The migration visits
// the apps in app_id order, because `WebAppRegistrar::Registry` is a
// `std::map`. If that container changes, the pair checks can fail. `opted_in`
// is the app that the user sets to "Open in app". `opted_in_first` selects if
// the migration visits `opted_in` before or after `other`. The app ids do not
// change, so the selection is the same in all `PRE_` steps.
struct SameScopePair {
  webapps::AppId opted_in;
  webapps::AppId other;
};

GURL SameScopeStartUrl(std::string_view host, std::string_view file_name) {
  return GURL(base::StrCat({"https://", host, "/app/", file_name}));
}

void InstallSameScopePair(Profile* profile, std::string_view host) {
  InstallAppWithOwnScope(profile, SameScopeStartUrl(host, "one.html"));
  InstallAppWithOwnScope(profile, SameScopeStartUrl(host, "two.html"));
}

SameScopePair GetSameScopePair(std::string_view host, bool opted_in_first) {
  webapps::AppId first = AppIdForStartUrl(SameScopeStartUrl(host, "one.html"));
  webapps::AppId second = AppIdForStartUrl(SameScopeStartUrl(host, "two.html"));
  if (second < first) {
    std::swap(first, second);
  }
  return opted_in_first ? SameScopePair{first, second}
                        : SameScopePair{second, first};
}

void ExpectExactlyOnePreferred(Profile* profile,
                               const webapps::AppId& app_id1,
                               const webapps::AppId& app_id2) {
  EXPECT_NE(IsPreferred(profile, app_id1), IsPreferred(profile, app_id2));
}

// Does the same as an ARC-side change (for example, in Android settings): App
// Service replaces `replaced_app_id` with a non-web app directly, and no record
// is written.
void SetNonWebAppPreferredForScope(Profile* profile,
                                   const GURL& start_url,
                                   const webapps::AppId& replaced_app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  apps_util::PreferredAppUpdateWaiter waiter(proxy->PreferredAppsList(),
                                             replaced_app_id,
                                             /*is_preferred_app=*/false);
  apps::IntentFilters filters;
  filters.push_back(
      apps_util::MakeIntentFilterForUrlScope(start_url.GetWithoutFilename()));
  proxy->SetSupportedLinksPreference(kNonWebAppId, std::move(filters));
  waiter.Wait();
}

}  // namespace

// b/578641259: The default-on migration keeps explicit user choices:
// 1. `PRE_*` (default-off): App A has no choice. The user selects "Open in
//    app" for App B and "Open in Chrome browser" for App C. App F is in the
//    server-side `user_settings_forced_off_apps` list.
// 2. Main test (default-on): App A gets the new default. App B stays
//    preferred. App C and App F stay in the browser, unless the kill switch is
//    off.
class WebAppNavigationCapturingMigrationUserChoiceTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  WebAppNavigationCapturingMigrationUserChoiceTest() {
    InitMigrationFeatures(
        feature_list_,
        CurrentTestPreCount() > 0 ? "reimpl_default_off" : "reimpl_default_on",
        /*respect_user_choice=*/GetParam(),
        /*forced_off_app_id=*/AppIdForStartUrl(GURL(kAppFStartUrl)));
  }

 protected:
  bool RespectUserChoice() const { return GetParam(); }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationUserChoiceTest,
                       PRE_KeepsExplicitChoice) {
  webapps::AppId app_a = InstallAppWithOwnScope(profile(), GURL(kAppAStartUrl));
  webapps::AppId app_b = InstallAppWithOwnScope(profile(), GURL(kAppBStartUrl));
  webapps::AppId app_c = InstallAppWithOwnScope(profile(), GURL(kAppCStartUrl));
  webapps::AppId app_f = InstallAppWithOwnScope(profile(), GURL(kAppFStartUrl));
  WaitForPreferredAppsList(profile());

  SimulateSettingsChoice(profile(), provider(), app_b, /*open_in_app=*/true);
  SimulateSettingsChoice(profile(), provider(), app_c, /*open_in_app=*/false);

  EXPECT_FALSE(IsPreferred(profile(), app_a));
  EXPECT_TRUE(IsPreferred(profile(), app_b));
  EXPECT_FALSE(IsPreferred(profile(), app_c));
  EXPECT_FALSE(IsPreferred(profile(), app_f));
  EXPECT_FALSE(provider().registrar_unsafe().CanCaptureLinksInScope(app_f));
  EXPECT_EQ(provider()
                .registrar_unsafe()
                .GetAppById(app_c)
                ->user_link_capturing_preference(),
            proto::NAVIGATION_CAPTURING_PREFERENCE_DO_NOT_CAPTURE);
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationUserChoiceTest,
                       KeepsExplicitChoice) {
  WaitForPreferredAppsList(profile());
  ASSERT_TRUE(WaitForMigrationState(profile(), MigrationState::kDefaultOn));

  webapps::AppId app_a = AppIdForStartUrl(GURL(kAppAStartUrl));
  webapps::AppId app_b = AppIdForStartUrl(GURL(kAppBStartUrl));
  webapps::AppId app_c = AppIdForStartUrl(GURL(kAppCStartUrl));
  webapps::AppId app_f = AppIdForStartUrl(GURL(kAppFStartUrl));

  // No user choice: gets the new default.
  EXPECT_TRUE(IsPreferred(profile(), app_a));
  // Explicit "Open in app": kept.
  EXPECT_TRUE(IsPreferred(profile(), app_b));
  // Explicit "Open in Chrome browser": kept, unless the kill switch is off.
  EXPECT_EQ(IsPreferred(profile(), app_c), !RespectUserChoice());
  // `CanCaptureLinksInScope()` is false: no default, unless the kill switch is
  // off.
  EXPECT_EQ(IsPreferred(profile(), app_f), !RespectUserChoice());

  const base::ListValue& backup = profile()->GetPrefs()->GetList(
      prefs::kWebAppsPreviouslyAppSupportedLinks);
  ASSERT_EQ(backup.size(), 1u);
  EXPECT_EQ(backup[0].GetString(), app_b);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppNavigationCapturingMigrationUserChoiceTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "RespectUserChoice"
                                             : "KillSwitchOff";
                         });

// b/578641259: Choices made after the migration survive a rollback and a
// second migration (for example, when the navigation capturing state changes
// from default-on to default-off and back).
// 1. `PRE_PRE_PRE_*` (default-off): Install App A, App E, App G, App H, App X,
//    and three pairs of same-scope apps. The user selects "Open in app" for
//    App G, App X, and one app of each pair. App H is preferred, but has no
//    record (a choice made before the record existed).
// 2. `PRE_PRE_*` (default-on): App A and App E get migrated. The preferred
//    apps stay preferred and go into the backup, and the migration does not
//    replace an app of a same-scope pair. Then the user selects "Open in Chrome
//    browser" for App A and App G, and turns App E off and on again. App H is
//    turned off with no record. An ARC-side change gives the links of App X to
//    a non-web app, with no record. In the third pair, the user selects "Open
//    in app" for the other app. It replaces the first app, which gets a "do
//    not capture" record.
// 3. `PRE_*` (default-off, rollback): App A stays off. App E stays on, unless
//    the kill switch is off. Backed-up apps that are not preferred now (App G,
//    App H, App X, and the replaced app of the third pair) stay off, unless the
//    kill switch is off. The opted-in apps of the first two pairs are restored.
// 4. Main test (default-on, second migration): App A and App G stay off, unless
//    the kill switch is off. App E stays on. App H has no record, so it gets
//    the default again. The non-web app keeps the links of App X. The
//    same-scope apps keep their state.
class WebAppNavigationCapturingMigrationFlagFlipTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  WebAppNavigationCapturingMigrationFlagFlipTest() {
    // An even number of PRE_ prefixes means default-on.
    InitMigrationFeatures(feature_list_,
                          CurrentTestPreCount() % 2 == 0 ? "reimpl_default_on"
                                                         : "reimpl_default_off",
                          /*respect_user_choice=*/GetParam());
  }

 protected:
  bool RespectUserChoice() const { return GetParam(); }

  // The user opted in the app that the migration visits first.
  SameScopePair OptedInFirstPair() const {
    return GetSameScopePair(kSameScopeOptedInFirstHost,
                            /*opted_in_first=*/true);
  }
  // The user opted in the app that the migration visits second.
  SameScopePair OptedInSecondPair() const {
    return GetSameScopePair(kSameScopeOptedInSecondHost,
                            /*opted_in_first=*/false);
  }
  // After the migration, the user selects "Open in app" for `other`, which
  // replaces `opted_in`.
  SameScopePair ReplacedPair() const {
    return GetSameScopePair(kSameScopeReplacedHost, /*opted_in_first=*/true);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationFlagFlipTest,
                       PRE_PRE_PRE_ChoiceSurvivesFlagFlip) {
  webapps::AppId app_a = InstallAppWithOwnScope(profile(), GURL(kAppAStartUrl));
  webapps::AppId app_e = InstallAppWithOwnScope(profile(), GURL(kAppEStartUrl));
  webapps::AppId app_g = InstallAppWithOwnScope(profile(), GURL(kAppGStartUrl));
  webapps::AppId app_h = InstallAppWithOwnScope(profile(), GURL(kAppHStartUrl));
  webapps::AppId app_x = InstallAppWithOwnScope(profile(), GURL(kAppXStartUrl));
  InstallSameScopePair(profile(), kSameScopeOptedInFirstHost);
  InstallSameScopePair(profile(), kSameScopeOptedInSecondHost);
  InstallSameScopePair(profile(), kSameScopeReplacedHost);
  WaitForPreferredAppsList(profile());
  EXPECT_FALSE(IsPreferred(profile(), app_a));
  EXPECT_FALSE(IsPreferred(profile(), app_e));

  SimulateSettingsChoice(profile(), provider(), app_g, /*open_in_app=*/true);
  SimulateSettingsChoice(profile(), provider(), app_x, /*open_in_app=*/true);
  // A choice made before the record existed: App Service state only.
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_h);
  for (const SameScopePair& pair :
       {OptedInFirstPair(), OptedInSecondPair(), ReplacedPair()}) {
    SimulateSettingsChoice(profile(), provider(), pair.opted_in,
                           /*open_in_app=*/true);
    EXPECT_TRUE(IsPreferred(profile(), pair.opted_in));
    EXPECT_FALSE(IsPreferred(profile(), pair.other));
  }
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationFlagFlipTest,
                       PRE_PRE_ChoiceSurvivesFlagFlip) {
  WaitForPreferredAppsList(profile());
  ASSERT_TRUE(WaitForMigrationState(profile(), MigrationState::kDefaultOn));
  webapps::AppId app_a = AppIdForStartUrl(GURL(kAppAStartUrl));
  webapps::AppId app_e = AppIdForStartUrl(GURL(kAppEStartUrl));
  webapps::AppId app_g = AppIdForStartUrl(GURL(kAppGStartUrl));
  webapps::AppId app_h = AppIdForStartUrl(GURL(kAppHStartUrl));
  webapps::AppId app_x = AppIdForStartUrl(GURL(kAppXStartUrl));
  const SameScopePair first = OptedInFirstPair();
  const SameScopePair second = OptedInSecondPair();
  const SameScopePair replaced = ReplacedPair();
  ASSERT_TRUE(IsPreferred(profile(), app_a));
  ASSERT_TRUE(IsPreferred(profile(), app_e));
  ASSERT_TRUE(IsPreferred(profile(), app_g));
  ASSERT_TRUE(IsPreferred(profile(), app_h));
  ASSERT_TRUE(IsPreferred(profile(), app_x));

  // The migration does not replace the opted-in app of a same-scope pair, for
  // both app_id orders. If the kill switch is off, the result depends on the
  // order.
  for (const SameScopePair& pair : {first, second, replaced}) {
    if (RespectUserChoice()) {
      EXPECT_TRUE(IsPreferred(profile(), pair.opted_in));
      EXPECT_FALSE(IsPreferred(profile(), pair.other));
    } else {
      ExpectExactlyOnePreferred(profile(), pair.opted_in, pair.other);
    }
  }
  // The backup has the apps that were preferred before the migration. If the
  // kill switch is off, `other` replaced `second.opted_in` before the
  // migration visited it, so it is not in the backup.
  const base::flat_set<std::string> backup = GetBackupAppIds(profile());
  EXPECT_FALSE(backup.contains(app_a));
  EXPECT_FALSE(backup.contains(app_e));
  EXPECT_TRUE(backup.contains(app_g));
  EXPECT_TRUE(backup.contains(app_h));
  EXPECT_TRUE(backup.contains(app_x));
  EXPECT_TRUE(backup.contains(first.opted_in));
  EXPECT_EQ(backup.contains(second.opted_in), RespectUserChoice());
  EXPECT_TRUE(backup.contains(replaced.opted_in));
  for (const SameScopePair& pair : {first, second, replaced}) {
    EXPECT_FALSE(backup.contains(pair.other));
  }

  SimulateSettingsChoice(profile(), provider(), app_a, /*open_in_app=*/false);
  SimulateSettingsChoice(profile(), provider(), app_e, /*open_in_app=*/false);
  SimulateSettingsChoice(profile(), provider(), app_e, /*open_in_app=*/true);
  SimulateSettingsChoice(profile(), provider(), app_g, /*open_in_app=*/false);
  // Turned off with no record (for example, before the record existed).
  apps_util::RemoveSupportedLinksPreferenceAndWait(profile(), app_h);
  SetNonWebAppPreferredForScope(profile(), GURL(kAppXStartUrl), app_x);
  SimulateSettingsChoice(profile(), provider(), replaced.other,
                         /*open_in_app=*/true);

  EXPECT_FALSE(IsPreferred(profile(), app_a));
  EXPECT_TRUE(IsPreferred(profile(), app_e));
  EXPECT_FALSE(IsPreferred(profile(), app_g));
  EXPECT_FALSE(IsPreferred(profile(), app_h));
  EXPECT_FALSE(IsPreferred(profile(), app_x));
  EXPECT_EQ(PreferredAppForUrl(profile(), GURL(kAppXStartUrl)),
            std::string(kNonWebAppId));
  EXPECT_TRUE(IsPreferred(profile(), replaced.other));
  EXPECT_FALSE(IsPreferred(profile(), replaced.opted_in));
  // The Settings choice replaced `replaced.opted_in`, so it recorded "Open in
  // Chrome browser" on it. If the kill switch is off, the migration already
  // gave the links to `replaced.other`, so the choice replaced no app.
  if (RespectUserChoice()) {
    EXPECT_EQ(provider()
                  .registrar_unsafe()
                  .GetAppById(replaced.opted_in)
                  ->user_link_capturing_preference(),
              proto::NAVIGATION_CAPTURING_PREFERENCE_DO_NOT_CAPTURE);
  }
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationFlagFlipTest,
                       PRE_ChoiceSurvivesFlagFlip) {
  WaitForPreferredAppsList(profile());
  ASSERT_TRUE(WaitForMigrationState(profile(), MigrationState::kDefaultOff));
  const SameScopePair first = OptedInFirstPair();
  const SameScopePair second = OptedInSecondPair();
  const SameScopePair replaced = ReplacedPair();

  // App A was not in the backup, and the user turned it off.
  EXPECT_FALSE(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppAStartUrl))));
  // The rollback keeps the explicit "Open in app" for App E, although App E is
  // not in the backup. If the kill switch is off, only the backup is used.
  EXPECT_EQ(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppEStartUrl))),
            RespectUserChoice());
  // App G, App H, and App X are in the backup, but are not preferred before
  // the rollback, so the rollback does not restore them. This is true with or
  // without a record. If the kill switch is off, only the backup is used.
  EXPECT_EQ(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppGStartUrl))),
            !RespectUserChoice());
  EXPECT_EQ(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppHStartUrl))),
            !RespectUserChoice());
  EXPECT_EQ(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppXStartUrl))),
            !RespectUserChoice());
  // The rollback does not replace the non-web app, unless the kill switch is
  // off.
  EXPECT_EQ(PreferredAppForUrl(profile(), GURL(kAppXStartUrl)),
            RespectUserChoice() ? std::string(kNonWebAppId)
                                : AppIdForStartUrl(GURL(kAppXStartUrl)));

  // The opted-in apps of the same-scope pairs are restored. If the kill switch
  // is off, `second.opted_in` is not restored, because it is not in the
  // backup.
  EXPECT_TRUE(IsPreferred(profile(), first.opted_in));
  EXPECT_FALSE(IsPreferred(profile(), first.other));
  EXPECT_EQ(IsPreferred(profile(), second.opted_in), RespectUserChoice());
  EXPECT_FALSE(IsPreferred(profile(), second.other));
  // The newer "Open in app" choice for `replaced.other` wins over the backup,
  // unless the kill switch is off.
  EXPECT_EQ(IsPreferred(profile(), replaced.other), RespectUserChoice());
  EXPECT_EQ(IsPreferred(profile(), replaced.opted_in), !RespectUserChoice());

  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(
      prefs::kWebAppsPreviouslyAppSupportedLinks));
  // TODO(crbug.com/537369804): Replace with a targeted waiter for disk writes.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationFlagFlipTest,
                       ChoiceSurvivesFlagFlip) {
  WaitForPreferredAppsList(profile());
  ASSERT_TRUE(WaitForMigrationState(profile(), MigrationState::kDefaultOn));
  // The second migration does not reset the explicit opt-outs, unless the kill
  // switch is off.
  EXPECT_EQ(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppAStartUrl))),
            !RespectUserChoice());
  EXPECT_TRUE(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppEStartUrl))));
  EXPECT_EQ(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppGStartUrl))),
            !RespectUserChoice());
  // App H has no record, so the second migration applies the default again.
  EXPECT_TRUE(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppHStartUrl))));
  // The migration does not replace the non-web app, unless the kill switch is
  // off (then the rollback restored App X).
  EXPECT_EQ(IsPreferred(profile(), AppIdForStartUrl(GURL(kAppXStartUrl))),
            !RespectUserChoice());

  const SameScopePair first = OptedInFirstPair();
  const SameScopePair second = OptedInSecondPair();
  const SameScopePair replaced = ReplacedPair();
  if (RespectUserChoice()) {
    EXPECT_TRUE(IsPreferred(profile(), first.opted_in));
    EXPECT_FALSE(IsPreferred(profile(), first.other));
    EXPECT_TRUE(IsPreferred(profile(), second.opted_in));
    EXPECT_FALSE(IsPreferred(profile(), second.other));
    EXPECT_TRUE(IsPreferred(profile(), replaced.other));
    EXPECT_FALSE(IsPreferred(profile(), replaced.opted_in));
  } else {
    for (const SameScopePair& pair : {first, second, replaced}) {
      ExpectExactlyOnePreferred(profile(), pair.opted_in, pair.other);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppNavigationCapturingMigrationFlagFlipTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "RespectUserChoice"
                                             : "KillSwitchOff";
                         });
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
