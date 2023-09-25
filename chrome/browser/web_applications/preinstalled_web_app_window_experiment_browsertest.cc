// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_window_experiment.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_window_experiment_utils.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/user_manager/user_names.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#endif

namespace web_app {
using base::Bucket;
using mojom::UserDisplayMode;
using testing::ElementsAre;
using testing::IsEmpty;
using UserGroup = features::PreinstalledWebAppWindowExperimentUserGroup;

static const auto& kUserGroupParam =
    preinstalled_web_app_window_experiment_utils::GetFeatureParam();

// ChromeOS only.
class PreinstalledWebAppWindowExperimentBrowserTest
    : public InProcessBrowserTest,
      public WebAppRegistrarObserver,
      public apps::PreferredAppsListHandle::Observer {
 public:
  PreinstalledWebAppWindowExperimentBrowserTest()
      : fake_web_app_provider_creator_(
            base::BindRepeating(&PreinstalledWebAppWindowExperimentBrowserTest::
                                    CreateFakeWebAppProvider,
                                base::Unretained(this))) {}

  ~PreinstalledWebAppWindowExperimentBrowserTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    // Was added by `PrepareBrowserCommandLineForTests`. Re-enable default
    // apps as we wish to test effects on them.
    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(&provider());

    preferred_apps_observation_.Observe(&proxy().PreferredAppsList());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    loopback_crosapi_ =
        std::make_unique<LoopbackCrosapiAppServiceProxy>(browser()->profile());
#endif
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();

    preferred_apps_observation_.Reset();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    loopback_crosapi_ = nullptr;
#endif
  }

  // WebAppRegistrarObserver:
  void OnWebAppUserDisplayModeChanged(
      const webapps::AppId& app_id,
      UserDisplayMode user_display_mode) override {
    recorded_display_mode_changes_[app_id] = user_display_mode;
  }

  // WebAppRegistrarObserver:
  void OnAppRegistrarDestroyed() override { registrar_observation_.Reset(); }

  // apps::PreferredAppsListHandle::Observer:
  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override {
    recorded_link_capturing_changes_[app_id] = is_preferred_app;
  }

  // apps::PreferredAppsListHandle::Observer:
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override {
    preferred_apps_observation_.Reset();
  }

  WebAppProvider& provider() {
    auto* provider = WebAppProvider::GetForWebApps(browser()->profile());
    DCHECK(provider);
    return *provider;
  }

  // Experiment waits for sync system to be ready to check eligibility.
  void SimulateSyncReady() {
    auto& fake_provider = static_cast<FakeWebAppProvider&>(provider());
    ON_CALL(fake_provider.processor(), IsTrackingMetadata())
        .WillByDefault(testing::Return(true));

    WebAppSyncBridge& sync_bridge = provider().sync_bridge_unsafe();
    sync_bridge.MergeFullSyncData(sync_bridge.CreateMetadataChangeList(),
                                  syncer::EntityChangeList());
  }

  PreinstalledWebAppWindowExperiment& experiment() {
    return provider()
        .preinstalled_web_app_manager()
        .GetWindowExperimentForTesting();
  }

  // Waits for experiment setup code to run.
  void AwaitExperimentSetup() {
    base::RunLoop run_loop;
    experiment().setup_done_for_testing().Post(FROM_HERE,
                                               run_loop.QuitClosure());
    run_loop.Run();
  }

  // Waits for initial preinstalled apps to install.
  void AwaitPreinstalledAppsInstalled() {
    base::RunLoop run_loop;
    experiment().preinstalled_apps_installed_for_testing().Post(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  apps::AppServiceProxy& proxy() {
    DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
        browser()->profile()));
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    DCHECK(proxy);
    return *proxy;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LoopbackCrosapiAppServiceProxy& loopback_crosapi() {
    DCHECK(loopback_crosapi_);
    return *loopback_crosapi_;
  }
#endif

  // Abstraction of `apps_util::RemoveSupportedLinksPreferenceAndWait`.
  // CrosAPI doesn't have this method so it doesn't exist on Lacros.
  void RemoveSupportedLinksPreferenceAndWait(const std::string& app_id) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    loopback_crosapi().RemoveSupportedLinksPreference(app_id);
    apps_util::PreferredAppUpdateWaiter(proxy().PreferredAppsList(), app_id,
                                        /*is_preferred_app=*/false)
        .Wait();
#else
    apps_util::RemoveSupportedLinksPreferenceAndWait(browser()->profile(),
                                                     app_id);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  std::map<webapps::AppId, UserDisplayMode> recorded_display_mode_changes_;
  std::map<webapps::AppId, bool> recorded_link_capturing_changes_;

 private:
  std::unique_ptr<KeyedService> CreateFakeWebAppProvider(Profile* profile) {
    auto provider = std::make_unique<FakeWebAppProvider>(profile);

    // Use default fakes for fake working sync system.
    provider->CreateFakeSubsystems();

    // Added by `CreateFakeSubsystems`. Re-enable default apps as
    // we wish to test effects on them.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDisableDefaultApps);

    // Real database is needed for persistence in PRE_ tests.
    provider->SetDatabaseFactory(
        std::make_unique<WebAppDatabaseFactory>(profile));

    // Prevent sync from reporting as ready until `SimulateSyncReady`.
    ON_CALL(provider->processor(), IsTrackingMetadata())
        .WillByDefault(testing::Return(false));

    provider->SetSynchronizePreinstalledAppsOnStartup(true);

    provider->Start();
    registrar_observation_.Observe(&provider->registrar_unsafe());
    return provider;
  }

  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver>
      registrar_observation_{this};

  base::ScopedObservation<apps::PreferredAppsListHandle,
                          apps::PreferredAppsListHandle::Observer>
      preferred_apps_observation_{this};

  FakeWebAppProviderCreator fake_web_app_provider_creator_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<LoopbackCrosapiAppServiceProxy> loopback_crosapi_;
#endif
};

namespace {
absl::optional<UserDisplayMode> ToDisplayMode(UserGroup user_group) {
  switch (user_group) {
    case UserGroup::kUnknown:
    case UserGroup::kControl:
      return absl::nullopt;
    case UserGroup::kWindow:
      return UserDisplayMode::kStandalone;
    case UserGroup::kTab:
      return UserDisplayMode::kBrowser;
  }
}

std::string UserGroupToString(UserGroup user_group) {
  return kUserGroupParam.GetName(user_group);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
std::string UserGroupToTitleCaseString(UserGroup user_group) {
  switch (user_group) {
    case UserGroup::kUnknown:
      return "Unknown";
    case UserGroup::kControl:
      return "Control";
    case UserGroup::kWindow:
      return "Window";
    case UserGroup::kTab:
      return "Tab";
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

std::string ParamToString(
    const testing::TestParamInfo<
        features::PreinstalledWebAppWindowExperimentUserGroup> param_info) {
  return UserGroupToString(param_info.param);
}

}  // namespace

class PreinstalledWebAppWindowExperimentBrowserTestWindow
    : public PreinstalledWebAppWindowExperimentBrowserTest {
 public:
  PreinstalledWebAppWindowExperimentBrowserTestWindow() {
    features_.InitWithFeaturesAndParameters(
        {{features::kPreinstalledWebAppWindowExperiment,
          {{kUserGroupParam.name,
            kUserGroupParam.GetName(UserGroup::kWindow)}}}},
        /*disabled_features=*/{});
  }

  base::test::ScopedFeatureList features_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// WebAppProvider exists in Guest sessions on Ash only, so it is sufficient to
// test Guest sessions on Ash only.
class PreinstalledWebAppWindowExperimentBrowserTestWindowGuest
    : public PreinstalledWebAppWindowExperimentBrowserTestWindow {
 public:
  PreinstalledWebAppWindowExperimentBrowserTestWindowGuest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PreinstalledWebAppWindowExperimentBrowserTestWindow::SetUpCommandLine(
        command_line);

    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
  }
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTestWindowGuest,
                       IneligibleDueToGuestProfile) {
  Profile& guest_profile = *browser()->profile();
  EXPECT_TRUE(guest_profile.IsGuestSession());

  // Allow eligibility check to happen.
  SimulateSyncReady();
  AwaitExperimentSetup();

  // User is ineligible.
  EXPECT_EQ(preinstalled_web_app_window_experiment_utils::GetUserGroupPref(
                browser()->profile()->GetPrefs()),
            UserGroup::kUnknown);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTestWindow,
                       IneligibleDueToNonRecentApp) {
  // Install an app and set install time as if installed a long time ago.
  webapps::AppId app_id = test::InstallDummyWebApp(
      browser()->profile(), "non-recent app", GURL("https://example.com"));
  auto& fake_provider = static_cast<FakeWebAppProvider&>(provider());
  WebApp* app = fake_provider.GetRegistrarMutable().GetAppByIdMutable(app_id);
  DCHECK(app);
  app->SetFirstInstallTime(base::Time::UnixEpoch());

  // Allow eligibility check to happen.
  SimulateSyncReady();
  AwaitExperimentSetup();

  // User is ineligible.
  EXPECT_EQ(preinstalled_web_app_window_experiment_utils::GetUserGroupPref(
                browser()->profile()->GetPrefs()),
            UserGroup::kUnknown);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTestWindow,
                       IneligibleDueToSyncInstalledApp) {
  // Install an app as if it came from sync.
  webapps::AppId app_id = test::InstallDummyWebApp(
      browser()->profile(), "app from sync", GURL("https://example.com"),
      webapps::WebappInstallSource::SYNC);

  // Allow eligibility check to happen.
  SimulateSyncReady();
  AwaitExperimentSetup();

  // User is ineligible.
  EXPECT_EQ(preinstalled_web_app_window_experiment_utils::GetUserGroupPref(
                browser()->profile()->GetPrefs()),
            UserGroup::kUnknown);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTestWindow,
                       IneligibleDueToPendingSyncInstalledApp) {
  // Install an app and set as if just received from sync.
  webapps::AppId app_id = test::InstallDummyWebApp(
      browser()->profile(), "non-recent app", GURL("https://example.com"));
  auto& fake_provider = static_cast<FakeWebAppProvider&>(provider());
  WebApp* app = fake_provider.GetRegistrarMutable().GetAppByIdMutable(app_id);
  DCHECK(app);
  app->SetIsFromSyncAndPendingInstallation(true);

  // Allow eligibility check to happen.
  SimulateSyncReady();
  AwaitExperimentSetup();

  // User is ineligible.
  EXPECT_EQ(preinstalled_web_app_window_experiment_utils::GetUserGroupPref(
                browser()->profile()->GetPrefs()),
            UserGroup::kUnknown);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTestWindow,
                       PreinstalledAppLaunchedBeforeExperiment) {
  // Set preinstalled apps before the test apps so they aren't removed.
  AwaitPreinstalledAppsInstalled();

  // Install an app and set launch time as if recently launched.
  webapps::AppId launched_app_id = test::InstallDummyWebApp(
      browser()->profile(), "launched app", GURL("https://example1.com"),
      webapps::WebappInstallSource::INTERNAL_DEFAULT);
  provider().sync_bridge_unsafe().SetAppLastLaunchTime(launched_app_id,
                                                       base::Time::Now());

  webapps::AppId unlaunched_app_id = test::InstallDummyWebApp(
      browser()->profile(), "unlaunched app", GURL("https://example2.com"),
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  // Allow eligibility check to happen.
  SimulateSyncReady();
  AwaitExperimentSetup();

  // App recorded as launched before experiment.
  EXPECT_TRUE(preinstalled_web_app_window_experiment_utils::
                  HasLaunchedAppBeforeExperiment(
                      launched_app_id, browser()->profile()->GetPrefs()));
  // Arbitrary other apps not recorded as launched before experiment.
  EXPECT_FALSE(preinstalled_web_app_window_experiment_utils::
                   HasLaunchedAppBeforeExperiment(
                       unlaunched_app_id, browser()->profile()->GetPrefs()));
  EXPECT_FALSE(preinstalled_web_app_window_experiment_utils::
                   HasLaunchedAppBeforeExperiment(
                       kGoogleDriveAppId, browser()->profile()->GetPrefs()));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTestWindow,
                       IgnoreOnPreferredAppChangedCallFromSetup) {
  base::HistogramTester histogram_tester;

  // Set preinstalled apps before the test app so it isn't removed.
  AwaitPreinstalledAppsInstalled();

  // Use a real preinstalled app if available, otherwise install a fake one.
  // webapps::AppId must match a known preinstalled app for metrics to be
  // recorded.
  if (!provider().registrar_unsafe().IsInstalled(kGoogleDriveAppId)) {
    // Install an app and set supported links preference so experiment setting
    // it won't cause any observations.
    webapps::AppId app_id = test::InstallDummyWebApp(
        browser()->profile(), "launched app",
        GURL("https://drive.google.com/?lfhs=2"),
        webapps::WebappInstallSource::INTERNAL_DEFAULT);
    ASSERT_EQ(app_id, kGoogleDriveAppId);
  }

  ASSERT_FALSE(proxy().PreferredAppsList().IsPreferredAppForSupportedLinks(
      kGoogleDriveAppId));

  // Allow eligibility check to happen.
  SimulateSyncReady();
  AwaitExperimentSetup();

  // Wait for (possibly asynchronous) preferred apps change from experiment
  // setup.
  apps_util::PreferredAppUpdateWaiter(proxy().PreferredAppsList(),
                                      kGoogleDriveAppId,
                                      /*is_preferred_app=*/true)
      .Wait();

  // No Link Capturing metrics should have been recorded.
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingEnabled", 0);
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingDisabled", 0);

  // Simulate subsequent preferred app change from user.
  RemoveSupportedLinksPreferenceAndWait(kGoogleDriveAppId);

  // Now the remove should be recorded.
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingEnabled", 0);
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingDisabled", 1);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTestWindow,
                       NoIgnoreOnPreferredAppChangedCallFromUser) {
  base::HistogramTester histogram_tester;

  // Set preinstalled apps before the test app so it isn't removed.
  AwaitPreinstalledAppsInstalled();

  // Use a real preinstalled app if available, otherwise install a fake one.
  // webapps::AppId must match a known preinstalled app for metrics to be
  // recorded.
  if (!provider().registrar_unsafe().IsInstalled(kGoogleDriveAppId)) {
    // Install an app and set supported links preference so experiment setting
    // it won't cause any observations.
    webapps::AppId app_id = test::InstallDummyWebApp(
        browser()->profile(), "launched app",
        GURL("https://drive.google.com/?lfhs=2"),
        webapps::WebappInstallSource::INTERNAL_DEFAULT);
    ASSERT_EQ(app_id, kGoogleDriveAppId);
  }

  apps_util::SetSupportedLinksPreferenceAndWait(browser()->profile(),
                                                kGoogleDriveAppId);
  ASSERT_TRUE(proxy().PreferredAppsList().IsPreferredAppForSupportedLinks(
      kGoogleDriveAppId));

  // Allow eligibility check to happen.
  SimulateSyncReady();
  AwaitExperimentSetup();

  // Simulate preferred app change from user, without first seeing one from
  // experiment setup.
  RemoveSupportedLinksPreferenceAndWait(kGoogleDriveAppId);

  // The disable should be recorded.
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingEnabled", 0);
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingDisabled", 1);

  // Simulate the user re-enabling link capturing.
  apps_util::SetSupportedLinksPreferenceAndWait(browser()->profile(),
                                                kGoogleDriveAppId);

  // The enable should be recorded.
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingEnabled", 1);
  histogram_tester.ExpectTotalCount(
      "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingDisabled", 1);
}

class PreinstalledWebAppWindowExperimentBrowserTestAll
    : public PreinstalledWebAppWindowExperimentBrowserTest,
      public testing::WithParamInterface<UserGroup> {
 public:
  PreinstalledWebAppWindowExperimentBrowserTestAll() {
    features_.InitWithFeaturesAndParameters(
        {{features::kPreinstalledWebAppWindowExperiment,
          {{kUserGroupParam.name, kUserGroupParam.GetName(GetParam())}}}},
        /*disabled_features=*/{});
  }

  UserGroup GetUserGroupTestParam() { return GetParam(); }

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppWindowExperimentBrowserTestAll,
                       EligibleUserSetsOverrides) {
  const WebAppRegistrar& registrar = provider().registrar_unsafe();

  SimulateSyncReady();
  AwaitExperimentSetup();

  // Also need to ensure preinstalled have finished installing because the
  // experiment code doesn't wait for that if UserGroup is kUnknown.
  AwaitPreinstalledAppsInstalled();

  EXPECT_EQ(preinstalled_web_app_window_experiment_utils::GetUserGroupPref(
                browser()->profile()->GetPrefs()),
            GetUserGroupTestParam());

  bool has_preinstalled_apps = false;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  has_preinstalled_apps = true;
#endif

  if (!has_preinstalled_apps) {
    EXPECT_THAT(recorded_display_mode_changes_, IsEmpty());
    EXPECT_THAT(recorded_link_capturing_changes_, IsEmpty());
    // No apps expected to be installed/installing.
    std::vector<webapps::AppId> app_ids;
    for (const WebApp& app : registrar.GetAppsIncludingStubs()) {
      app_ids.emplace_back(app.app_id());
    }
    EXPECT_THAT(app_ids, IsEmpty());

    // Remaining tests rely on apps being preinstalled.
    return;
  }

  if (GetUserGroupTestParam() == UserGroup::kWindow) {
    // Check some arbitrary preinstalled apps have link capturing set.
    ASSERT_TRUE(recorded_link_capturing_changes_.contains(kGoogleDriveAppId));
    EXPECT_TRUE(recorded_link_capturing_changes_[kGoogleDriveAppId]);
    ASSERT_TRUE(recorded_link_capturing_changes_.contains(kYoutubeAppId));
    EXPECT_TRUE(recorded_link_capturing_changes_[kYoutubeAppId]);
    ASSERT_TRUE(
        recorded_link_capturing_changes_.contains(kGoogleCalendarAppId));
    EXPECT_TRUE(recorded_link_capturing_changes_[kGoogleCalendarAppId]);
  } else {
    EXPECT_THAT(recorded_link_capturing_changes_, IsEmpty());
  }

  absl::optional<UserDisplayMode> expected =
      ToDisplayMode(GetUserGroupTestParam());

  // Check some arbitrary preinstalled apps have display mode overridden on
  // the registrar, but their real user display mode stored on the web app
  // struct remains the default.
  //
  // Drive defaults to browser.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).value(),
            expected.value_or(UserDisplayMode::kBrowser));
  EXPECT_EQ(registrar.GetAppById(kGoogleDriveAppId)->user_display_mode(),
            UserDisplayMode::kBrowser);
  if (expected.has_value()) {
    ASSERT_TRUE(recorded_display_mode_changes_.contains(kGoogleDriveAppId));
    EXPECT_EQ(recorded_display_mode_changes_[kGoogleDriveAppId],
              expected.value());
  } else {
    EXPECT_FALSE(recorded_display_mode_changes_.contains(kGoogleDriveAppId));
  }
  // Youtube defaults to standalone on CrOS.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kYoutubeAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kYoutubeAppId).value(),
            expected.value_or(UserDisplayMode::kStandalone));
  EXPECT_EQ(registrar.GetAppById(kYoutubeAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);
  if (expected.has_value()) {
    ASSERT_TRUE(recorded_display_mode_changes_.contains(kYoutubeAppId));
    EXPECT_EQ(recorded_display_mode_changes_[kYoutubeAppId], expected.value());
  } else {
    EXPECT_FALSE(recorded_display_mode_changes_.contains(kYoutubeAppId));
  }
  // Calendar defaults to standalone on CrOS.
  ASSERT_TRUE(
      registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).value(),
            expected.value_or(UserDisplayMode::kStandalone));
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);
  if (expected.has_value()) {
    ASSERT_TRUE(recorded_display_mode_changes_.contains(kGoogleCalendarAppId));
    EXPECT_EQ(recorded_display_mode_changes_[kGoogleCalendarAppId],
              expected.value());
  } else {
    EXPECT_FALSE(recorded_display_mode_changes_.contains(kGoogleCalendarAppId));
  }

  recorded_display_mode_changes_.clear();

  // Simulate user setting the display mode on some apps.
  provider().sync_bridge_unsafe().SetAppUserDisplayMode(
      kGoogleDriveAppId, UserDisplayMode::kStandalone, /*is_user_action=*/true);
  provider().sync_bridge_unsafe().SetAppUserDisplayMode(
      kYoutubeAppId, UserDisplayMode::kStandalone, /*is_user_action=*/true);
  provider().sync_bridge_unsafe().SetAppUserDisplayMode(
      kGoogleCalendarAppId, UserDisplayMode::kBrowser, /*is_user_action=*/true);

  // UI should be notified with the new values.
  ASSERT_TRUE(recorded_display_mode_changes_.contains(kGoogleDriveAppId));
  EXPECT_EQ(recorded_display_mode_changes_[kGoogleDriveAppId],
            UserDisplayMode::kStandalone);
  ASSERT_TRUE(recorded_display_mode_changes_.contains(kYoutubeAppId));
  EXPECT_EQ(recorded_display_mode_changes_[kYoutubeAppId],
            UserDisplayMode::kStandalone);
  ASSERT_TRUE(recorded_display_mode_changes_.contains(kGoogleCalendarAppId));
  EXPECT_EQ(recorded_display_mode_changes_[kGoogleCalendarAppId],
            UserDisplayMode::kBrowser);

  // User-overridden apps should be recorded.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  base::flat_set<webapps::AppId> overridden_apps =
      preinstalled_web_app_window_experiment_utils::
          GetAppIdsWithUserOverridenDisplayModePref(pref_service);
  if (GetUserGroupTestParam() == UserGroup::kUnknown) {
    EXPECT_THAT(overridden_apps, IsEmpty());
  } else {
    EXPECT_EQ(overridden_apps,
              base::flat_set<webapps::AppId>(
                  {kGoogleDriveAppId, kYoutubeAppId, kGoogleCalendarAppId}));
  }

  // Registrar should now return user-set values.
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kYoutubeAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).value(),
            UserDisplayMode::kBrowser);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_P(PreinstalledWebAppWindowExperimentBrowserTestAll,
                       RecordsHistograms) {
  base::HistogramTester histogram_tester;
  std::string user_group = UserGroupToTitleCaseString(GetUserGroupTestParam());

  SimulateSyncReady();
  AwaitExperimentSetup();

  // Also need to ensure preinstalled have finished installing because the
  // experiment code doesn't wait for that if UserGroup is UNKNOWN.
  AwaitPreinstalledAppsInstalled();

  // Simulate user setting the display mode on some apps.
  provider().sync_bridge_unsafe().SetAppUserDisplayMode(
      kGoogleDriveAppId, UserDisplayMode::kStandalone, /*is_user_action=*/true);
  provider().sync_bridge_unsafe().SetAppUserDisplayMode(
      kYoutubeAppId, UserDisplayMode::kStandalone, /*is_user_action=*/true);
  provider().sync_bridge_unsafe().SetAppUserDisplayMode(
      kGoogleCalendarAppId, UserDisplayMode::kBrowser, /*is_user_action=*/true);

  // Metrics should be emitted iff experiment is active.
  if (GetUserGroupTestParam() == UserGroup::kUnknown) {
    histogram_tester.ExpectTotalCount(
        base::StrCat({"WebApp.Preinstalled.WindowExperiment.", user_group,
                      ".ChangedToWindow"}),
        0);
    histogram_tester.ExpectTotalCount(
        base::StrCat({"WebApp.Preinstalled.WindowExperiment.", user_group,
                      ".ChangedToTab"}),
        0);
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    base::StrCat({"WebApp.Preinstalled.WindowExperiment.",
                                  user_group, ".ChangedToWindow"})),
                ElementsAre(Bucket(apps::DefaultAppName::kDrive, 1),
                            Bucket(apps::DefaultAppName::kYouTube, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    base::StrCat({"WebApp.Preinstalled.WindowExperiment.",
                                  user_group, ".ChangedToTab"})),
                ElementsAre(Bucket(apps::DefaultAppName::kGoogleCalendar, 1)));
  }

  // Simulate user changing link capturing on some apps.
  Profile* profile = browser()->profile();
  apps_util::SetSupportedLinksPreferenceAndWait(profile, kGoogleDriveAppId);
  apps_util::SetSupportedLinksPreferenceAndWait(profile, kYoutubeAppId);
  RemoveSupportedLinksPreferenceAndWait(kGoogleCalendarAppId);

  // Metrics should be emitted iff experiment is active and link capturing
  // state actually changed.
  switch (GetUserGroupTestParam()) {
    // Capturing already enabled or experiment disabled.
    case UserGroup::kUnknown:
    case UserGroup::kWindow:
      histogram_tester.ExpectTotalCount(
          base::StrCat({"WebApp.Preinstalled.WindowExperiment.", user_group,
                        ".LinkCapturingEnabled"}),
          0);
      break;
    case UserGroup::kControl:
    case UserGroup::kTab:
      EXPECT_THAT(histogram_tester.GetAllSamples(
                      base::StrCat({"WebApp.Preinstalled.WindowExperiment.",
                                    user_group, ".LinkCapturingEnabled"})),
                  ElementsAre(Bucket(apps::DefaultAppName::kDrive, 1),
                              Bucket(apps::DefaultAppName::kYouTube, 1)));
      break;
  }

  switch (GetUserGroupTestParam()) {
    // Capturing already disabled or experiment disabled.
    case UserGroup::kUnknown:
    case UserGroup::kControl:
    case UserGroup::kTab:
      histogram_tester.ExpectTotalCount(
          base::StrCat({"WebApp.Preinstalled.WindowExperiment.", user_group,
                        ".LinkCapturingDisabled"}),
          0);
      break;
    case UserGroup::kWindow:
      EXPECT_THAT(
          histogram_tester.GetAllSamples(
              base::StrCat({"WebApp.Preinstalled.WindowExperiment.", user_group,
                            ".LinkCapturingDisabled"})),
          ElementsAre(Bucket(apps::DefaultAppName::kGoogleCalendar, 1)));
      break;
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         PreinstalledWebAppWindowExperimentBrowserTestAll,
                         testing::Values(UserGroup::kUnknown,
                                         UserGroup::kControl,
                                         UserGroup::kWindow,
                                         UserGroup::kTab),
                         &ParamToString);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTest,
                       PRE_PersistStateWhenExperimentEnds_Window) {
  AwaitExperimentSetup();
  AwaitPreinstalledAppsInstalled();

  // Simulate experiment having run previously on this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  preinstalled_web_app_window_experiment_utils::SetUserGroupPref(
      pref_service, UserGroup::kWindow);
  // Simulate user manually setting some apps to their current value (`kBrowser`
  // for Drive, `kStandalone` for YouTube).
  preinstalled_web_app_window_experiment_utils::SetUserOverridenDisplayModePref(
      pref_service, kGoogleDriveAppId);
  preinstalled_web_app_window_experiment_utils::SetUserOverridenDisplayModePref(
      pref_service, kYoutubeAppId);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTest,
                       PersistStateWhenExperimentEnds_Window) {
  AwaitExperimentSetup();

  const WebAppRegistrar& registrar = provider().registrar_unsafe();

  // Experiment now disabled, so pref values should be persisted to web app DB,
  // unless the user set the values.

  // Drive left as "user set" value `kBrowser`.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).value(),
            UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar.GetAppById(kGoogleDriveAppId)->user_display_mode(),
            UserDisplayMode::kBrowser);
  // YouTube left as "user set" value `kStandalone`.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kYoutubeAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kYoutubeAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppById(kYoutubeAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);
  // Other apps set to experiment value `kStandalone`.
  ASSERT_TRUE(
      registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kGoogleSheetsAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleSheetsAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppById(kGoogleSheetsAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);

  // Pref should be deleted.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  EXPECT_TRUE(
      pref_service->GetDict("web_apps.preinstalled_app_window_experiment")
          .empty());
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTest,
                       PRE_PersistStateWhenExperimentEnds_Tab) {
  AwaitExperimentSetup();
  AwaitPreinstalledAppsInstalled();

  // Simulate experiment having run previously on this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  preinstalled_web_app_window_experiment_utils::SetUserGroupPref(
      pref_service, UserGroup::kTab);
  // Simulate user manually setting some apps to their current value (`kBrowser`
  // for Drive, `kStandalone` for YouTube).
  preinstalled_web_app_window_experiment_utils::SetUserOverridenDisplayModePref(
      pref_service, kGoogleDriveAppId);
  preinstalled_web_app_window_experiment_utils::SetUserOverridenDisplayModePref(
      pref_service, kYoutubeAppId);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTest,
                       PersistStateWhenExperimentEnds_Tab) {
  AwaitExperimentSetup();

  const WebAppRegistrar& registrar = provider().registrar_unsafe();

  // Experiment now disabled, so pref values should be persisted to web app DB,
  // unless the user set the values.

  // Drive left as "user set" value `kBrowser`.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).value(),
            UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar.GetAppById(kGoogleDriveAppId)->user_display_mode(),
            UserDisplayMode::kBrowser);
  // YouTube left as "user set" value `kStandalone`.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kYoutubeAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kYoutubeAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppById(kYoutubeAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);
  // Other apps set to experiment value `kBrowser`.
  ASSERT_TRUE(
      registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).value(),
            UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->user_display_mode(),
            UserDisplayMode::kBrowser);
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kGoogleSheetsAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleSheetsAppId).value(),
            UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar.GetAppById(kGoogleSheetsAppId)->user_display_mode(),
            UserDisplayMode::kBrowser);

  // Pref should be deleted.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  EXPECT_TRUE(
      pref_service->GetDict("web_apps.preinstalled_app_window_experiment")
          .empty());
}

class PreinstalledWebAppWindowExperimentBrowserTest_NoCleanup
    : public PreinstalledWebAppWindowExperimentBrowserTest {
 public:
  PreinstalledWebAppWindowExperimentBrowserTest_NoCleanup() {
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kWebAppWindowExperimentCleanup});
  }

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTest_NoCleanup,
                       PRE_NoPersistStateWhenExperimentEnds) {
  AwaitExperimentSetup();
  AwaitPreinstalledAppsInstalled();

  // Simulate experiment having run previously on this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  preinstalled_web_app_window_experiment_utils::SetUserGroupPref(
      pref_service, UserGroup::kWindow);
  // Simulate user manually setting some apps to their current value (`kBrowser`
  // for Drive, `kStandalone` for YouTube).
  preinstalled_web_app_window_experiment_utils::SetUserOverridenDisplayModePref(
      pref_service, kGoogleDriveAppId);
  preinstalled_web_app_window_experiment_utils::SetUserOverridenDisplayModePref(
      pref_service, kYoutubeAppId);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppWindowExperimentBrowserTest_NoCleanup,
                       NoPersistStateWhenExperimentEnds) {
  AwaitExperimentSetup();

  const WebAppRegistrar& registrar = provider().registrar_unsafe();

  // Experiment now disabled, but cleanup is also disabled, so pref values
  // should not be persisted to web app DB. Apps should have their default
  // values.
  // Drive defaults to browser.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleDriveAppId).value(),
            UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar.GetAppById(kGoogleDriveAppId)->user_display_mode(),
            UserDisplayMode::kBrowser);
  // YouTube defaults to standalone.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kYoutubeAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kYoutubeAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppById(kYoutubeAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);
  // Calendar defaults to standalone.
  ASSERT_TRUE(
      registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleCalendarAppId).value(),
            UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->user_display_mode(),
            UserDisplayMode::kStandalone);
  // Sheets defaults to browser.
  ASSERT_TRUE(registrar.GetAppUserDisplayMode(kGoogleSheetsAppId).has_value());
  EXPECT_EQ(registrar.GetAppUserDisplayMode(kGoogleSheetsAppId).value(),
            UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar.GetAppById(kGoogleSheetsAppId)->user_display_mode(),
            UserDisplayMode::kBrowser);

  // Pref should not be deleted.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  EXPECT_FALSE(
      pref_service->GetDict("web_apps.preinstalled_app_window_experiment")
          .empty());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace web_app
