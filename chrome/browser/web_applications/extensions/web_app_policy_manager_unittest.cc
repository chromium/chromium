// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager_observer.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#endif  // defined(OS_CHROMEOS)

using sync_preferences::TestingPrefServiceSyncable;

namespace web_app {

namespace {

const char kWebAppSettingWithDefaultConfiguration[] = R"({
  "https://windowed.example/": {
    "run_on_os_login": "run_windowed"
  },
  "https://tabbed.example/": {
    "run_on_os_login": "allowed"
  },
  "https://no-container.example/" : {
    "run_on_os_login": "unsupported_value"
  },
  "bad.uri" : {
    "run_on_os_login": "allowed"
  },
  "*": {
    "run_on_os_login": "blocked"
  }
})";

const char kDefaultFallbackAppName[] = "fallback app name";

constexpr char kWindowedUrl[] = "https://windowed.example/";
constexpr char kTabbedUrl[] = "https://tabbed.example/";
constexpr char kNoContainerUrl[] = "https://no-container.example/";

base::Value GetWindowedItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kWindowedUrl));
  item.SetKey(kDefaultLaunchContainerKey,
              base::Value(kDefaultLaunchContainerWindowValue));
  return item;
}

ExternalInstallOptions GetWindowedInstallOptions() {
  ExternalInstallOptions options(GURL(kWindowedUrl), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetTabbedItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kTabbedUrl));
  item.SetKey(kDefaultLaunchContainerKey,
              base::Value(kDefaultLaunchContainerTabValue));
  return item;
}

ExternalInstallOptions GetTabbedInstallOptions() {
  ExternalInstallOptions options(GURL(kTabbedUrl), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetNoContainerItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl));
  return item;
}

ExternalInstallOptions GetNoContainerInstallOptions() {
  ExternalInstallOptions options(GURL(kNoContainerUrl), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetCreateDesktopShortcutDefaultItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl));
  return item;
}

ExternalInstallOptions GetCreateDesktopShortcutDefaultInstallOptions() {
  ExternalInstallOptions options(GURL(kNoContainerUrl), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetCreateDesktopShortcutFalseItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl));
  item.SetKey(kCreateDesktopShortcutKey, base::Value(false));
  return item;
}

ExternalInstallOptions GetCreateDesktopShortcutFalseInstallOptions() {
  ExternalInstallOptions options(GURL(kNoContainerUrl), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetCreateDesktopShortcutTrueItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl));
  item.SetKey(kCreateDesktopShortcutKey, base::Value(true));
  return item;
}

ExternalInstallOptions GetCreateDesktopShortcutTrueInstallOptions() {
  ExternalInstallOptions options(GURL(kNoContainerUrl), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

class MockWebAppPolicyManagerObserver : public WebAppPolicyManagerObserver {
 public:
  void OnPolicyChanged() override { on_policy_changed_call_count++; }

  int GetOnPolicyChangedCalledCount() const {
    return on_policy_changed_call_count;
  }

 private:
  int on_policy_changed_call_count = 0;
};

base::Value GetFallbackAppNameItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kWindowedUrl));
  item.SetKey(kDefaultLaunchContainerKey,
              base::Value(kDefaultLaunchContainerWindowValue));
  item.SetKey(kFallbackAppNameKey, base::Value(kDefaultFallbackAppName));
  return item;
}

ExternalInstallOptions GetFallbackAppNameInstallOptions() {
  ExternalInstallOptions options(GURL(kWindowedUrl), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  options.fallback_app_name = kDefaultFallbackAppName;
  return options;
}

Source::Type ConvertExternalInstallSourceToSourceType(
    ExternalInstallSource external_install_source) {
  return InferSourceFromMetricsInstallSource(
      ConvertExternalInstallSourceToInstallSource(external_install_source));
}

}  // namespace

enum class TestParam { kLacrosDisabled, kLacrosEnabled };

class WebAppPolicyManagerTest : public ChromeRenderViewHostTestHarness,
                                public testing::WithParamInterface<TestParam> {
 public:
  WebAppPolicyManagerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  WebAppPolicyManagerTest(const WebAppPolicyManagerTest&) = delete;
  WebAppPolicyManagerTest& operator=(const WebAppPolicyManagerTest&) = delete;
  ~WebAppPolicyManagerTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (GetParam() == TestParam::kLacrosEnabled) {
      scoped_feature_list_.InitAndEnableFeature(features::kWebAppsCrosapi);
    } else if (GetParam() == TestParam::kLacrosDisabled) {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kWebAppsCrosapi, chromeos::features::kLacrosPrimary});
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    ChromeRenderViewHostTestHarness::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());
    fake_externally_managed_app_manager_ =
        std::make_unique<FakeExternallyManagedAppManager>(profile());
    test_system_app_manager_ =
        std::make_unique<web_app::TestSystemWebAppManager>(profile());
    web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(profile());

    controller().SetUp(profile());

    externally_managed_app_manager().SetSubsystems(
        &app_registrar(), &controller().os_integration_manager(), nullptr,
        nullptr, nullptr);
    externally_managed_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const ExternalInstallOptions& install_options) {
              const GURL& install_url = install_options.install_url;
              if (!app_registrar().GetAppById(GenerateAppId(
                      /*manifest_id=*/absl::nullopt, install_url))) {
                const auto install_source = install_options.install_source;
                std::unique_ptr<WebApp> web_app = test::CreateWebApp(
                    install_url,
                    ConvertExternalInstallSourceToSourceType(install_source));
                RegisterApp(std::move(web_app));

                externally_installed_app_prefs().Insert(
                    install_url,
                    GenerateAppId(/*manifest_id=*/absl::nullopt, install_url),
                    install_source);
              }
              return ExternallyManagedAppManager::InstallResult(
                  install_result_code_);
            }));
    externally_managed_app_manager().SetHandleUninstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const GURL& app_url,
                   ExternalInstallSource install_source) -> bool {
              absl::optional<AppId> app_id =
                  app_registrar().LookupExternalAppId(app_url);
              if (app_id) {
                UnregisterApp(*app_id);
              }
              return true;
            }));

    policy_manager().SetSubsystems(
        &externally_managed_app_manager(), &app_registrar(),
        &controller().sync_bridge(), &system_app_manager(),
        &controller().os_integration_manager());

    controller().Init();
  }

  void TearDown() override {
    web_app_policy_manager_.reset();
    test_system_app_manager_.reset();
    fake_externally_managed_app_manager_.reset();
    externally_installed_app_prefs_.reset();
    fake_registry_controller_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SimulatePreviouslyInstalledApp(GURL url,
                                      ExternalInstallSource install_source) {
    auto web_app = test::CreateWebApp(
        url, ConvertExternalInstallSourceToSourceType(install_source));
    RegisterApp(std::move(web_app));

    externally_installed_app_prefs().Insert(
        url, GenerateAppId(/*manifest_id=*/absl::nullopt, url), install_source);
  }

  void AwaitPolicyManagerAppsSynchronized() {
    base::RunLoop loop;
    policy_manager().SetOnAppsSynchronizedCompletedCallbackForTesting(
        loop.QuitClosure());
    loop.Run();
  }

  void AwaitPolicyManagerRefreshPolicySettings() {
    base::RunLoop loop;
    policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
        loop.QuitClosure());
    loop.Run();
  }

 protected:
  bool ShouldSkipPWASpecificTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (GetParam() == TestParam::kLacrosEnabled)
      return true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return false;
  }

  FakeExternallyManagedAppManager& externally_managed_app_manager() {
    return *fake_externally_managed_app_manager_;
  }

  TestSystemWebAppManager& system_app_manager() {
    return *test_system_app_manager_;
  }

  WebAppRegistrar& app_registrar() { return controller().registrar(); }
  WebAppPolicyManager& policy_manager() { return *web_app_policy_manager_; }
  ScopedTestingLocalState testing_local_state_;

  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  void SetWebAppSettingsDictPref(const base::StringPiece pref) {
    base::JSONReader::ValueWithError result =
        base::JSONReader::ReadAndReturnValueWithError(
            pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(result.value && result.value->is_dict())
        << result.error_message;
    profile()->GetPrefs()->Set(prefs::kWebAppSettings,
                               std::move(*result.value));
  }

  void ValidateEmptyWebAppSettingsPolicy() {
    EXPECT_TRUE(policy_manager().settings_by_url_.empty());
    ASSERT_TRUE(policy_manager().default_settings_);

    WebAppPolicyManager::WebAppSetting expected_default;
    EXPECT_EQ(policy_manager().default_settings_->run_on_os_login_policy,
              expected_default.run_on_os_login_policy);
  }

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    controller().RegisterApp(std::move(web_app));
  }

  void UnregisterApp(const AppId& app_id) {
    controller().UnregisterApp(app_id);
  }

  void SetInstallResultCode(InstallResultCode result_code) {
    install_result_code_ = result_code;
  }

 private:
  InstallResultCode install_result_code_ =
      InstallResultCode::kSuccessNewInstall;

  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;
  std::unique_ptr<FakeExternallyManagedAppManager>
      fake_externally_managed_app_manager_;
  std::unique_ptr<TestSystemWebAppManager> test_system_app_manager_;
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppPolicyManagerTest, NoPrefValues) {
  if (ShouldSkipPWASpecificTest())
    return;
  policy_manager().Start();

  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();
  EXPECT_TRUE(install_requests.empty());
  ValidateEmptyWebAppSettingsPolicy();
}

TEST_P(WebAppPolicyManagerTest, NoForceInstalledApps) {
  if (ShouldSkipPWASpecificTest())
    return;
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             base::Value(base::Value::Type::LIST));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();
  EXPECT_TRUE(install_requests.empty());
}

TEST_P(WebAppPolicyManagerTest, NoWebAppSettings) {
  if (ShouldSkipPWASpecificTest())
    return;
  profile()->GetPrefs()->Set(prefs::kWebAppSettings,
                             base::Value(base::Value::Type::DICTIONARY));

  policy_manager().Start();
  AwaitPolicyManagerRefreshPolicySettings();
  ValidateEmptyWebAppSettingsPolicy();
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsInvalidDefaultConfiguration) {
  if (ShouldSkipPWASpecificTest())
    return;
  const char kWebAppSettingInvalidDefaultConfiguration[] = R"({
    "*" : {
      "run_on_os_login": "unsupported_value"
    }
  })";

  SetWebAppSettingsDictPref(kWebAppSettingInvalidDefaultConfiguration);
  policy_manager().Start();
  AwaitPolicyManagerRefreshPolicySettings();
  ValidateEmptyWebAppSettingsPolicy();
}

TEST_P(WebAppPolicyManagerTest,
       WebAppSettingsInvalidDefaultConfigurationWithValidAppPolicy) {
  if (ShouldSkipPWASpecificTest())
    return;
  const char kWebAppSettingInvalidDefaultConfiguration[] = R"({
    "https://windowed.example/": {
      "run_on_os_login": "run_windowed"
    },
    "*" : {
      "run_on_os_login": "unsupported_value"
    }
  })";

  SetWebAppSettingsDictPref(kWebAppSettingInvalidDefaultConfiguration);
  policy_manager().Start();
  AwaitPolicyManagerRefreshPolicySettings();
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(
      policy_manager().GetUrlRunOnOsLoginPolicy(GURL("http://foo.example")),
      RunOnOsLoginPolicy::kAllowed);
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsNoDefaultConfiguration) {
  if (ShouldSkipPWASpecificTest())
    return;
  const char kWebAppSettingNoDefaultConfiguration[] = R"({
    "https://windowed.example/": {
      "run_on_os_login": "run_windowed"
    },
    "https://tabbed.example/": {
      "run_on_os_login": "blocked"
    },
    "https://no-container.example/" : {
      "run_on_os_login": "unsupported_value"
    },
    "bad.uri" : {
      "run_on_os_login": "allowed"
    }
  })";

  SetWebAppSettingsDictPref(kWebAppSettingNoDefaultConfiguration);
  policy_manager().Start();
  AwaitPolicyManagerRefreshPolicySettings();

  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(
      policy_manager().GetUrlRunOnOsLoginPolicy(GURL("http://foo.example")),
      RunOnOsLoginPolicy::kAllowed);
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsWithDefaultConfiguration) {
  if (ShouldSkipPWASpecificTest())
    return;
  SetWebAppSettingsDictPref(kWebAppSettingWithDefaultConfiguration);
  policy_manager().Start();
  AwaitPolicyManagerRefreshPolicySettings();

  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(
      policy_manager().GetUrlRunOnOsLoginPolicy(GURL("http://foo.example")),
      RunOnOsLoginPolicy::kBlocked);
}

TEST_P(WebAppPolicyManagerTest, TwoForceInstalledApps) {
  if (ShouldSkipPWASpecificTest())
    return;
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithNoDefaultLaunchContainer) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value list(base::Value::Type::LIST);
  list.Append(GetNoContainerItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetNoContainerInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest,
       ForceInstallAppWithDefaultCreateDesktopShortcut) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value list(base::Value::Type::LIST);
  list.Append(GetCreateDesktopShortcutDefaultItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(
      GetCreateDesktopShortcutDefaultInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithCreateDesktopShortcut) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value list(base::Value::Type::LIST);
  list.Append(GetCreateDesktopShortcutFalseItem());
  list.Append(GetCreateDesktopShortcutTrueItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(
      GetCreateDesktopShortcutFalseInstallOptions());
  expected_install_options_list.push_back(
      GetCreateDesktopShortcutTrueInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithFallbackAppName) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value list(base::Value::Type::LIST);
  list.Append(GetFallbackAppNameItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetFallbackAppNameInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, DynamicRefresh) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value first_list(base::Value::Type::LIST);
  first_list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  base::Value second_list(base::Value::Type::LIST);
  second_list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(second_list));

  base::RunLoop().RunUntilIdle();

  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, UninstallAppInstalledInPreviousSession) {
  if (ShouldSkipPWASpecificTest())
    return;
  // Simulate two policy apps and a regular app that were installed in the
  // previous session.
  SimulatePreviouslyInstalledApp(GURL(kWindowedUrl),
                                 ExternalInstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(GURL(kTabbedUrl),
                                 ExternalInstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(GURL(kNoContainerUrl),
                                 ExternalInstallSource::kInternalDefault);

  // Push a policy with only one of the apps.
  base::Value first_list(base::Value::Type::LIST);
  first_list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  // We should only try to install the app in the policy.
  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  EXPECT_EQ(externally_managed_app_manager().install_requests(),
            expected_install_options_list);

  // We should try to uninstall the app that is no longer in the policy.
  EXPECT_EQ(std::vector<GURL>({GURL(kTabbedUrl)}),
            externally_managed_app_manager().uninstall_requests());
}

// Tests that we correctly uninstall an app that we installed in the same
// session.
TEST_P(WebAppPolicyManagerTest, UninstallAppInstalledInCurrentSession) {
  if (ShouldSkipPWASpecificTest())
    return;
  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value first_list(base::Value::Type::LIST);
  first_list.Append(GetWindowedItem());
  first_list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));
  base::RunLoop().RunUntilIdle();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  // Push a new policy without the tabbed site.
  base::Value second_list(base::Value::Type::LIST);
  second_list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(second_list));
  base::RunLoop().RunUntilIdle();

  // We'll try to install the app again but ExternallyManagedAppManager will
  // handle not re-installing the app.
  expected_install_options_list.push_back(GetWindowedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  EXPECT_EQ(std::vector<GURL>({GURL(kTabbedUrl)}),
            externally_managed_app_manager().uninstall_requests());
}

// Tests that we correctly reinstall a placeholder app.
TEST_P(WebAppPolicyManagerTest, ReinstallPlaceholderApp) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl));
  base::RunLoop().RunUntilIdle();

  auto reinstall_options = GetWindowedInstallOptions();
  reinstall_options.install_placeholder = false;
  reinstall_options.reinstall_placeholder = true;
  reinstall_options.wait_for_windows_closed = true;
  expected_options_list.push_back(std::move(reinstall_options));

  EXPECT_EQ(expected_options_list, install_options_list);
}

// Tests that we correctly reinstall a placeholder app when the placeholder
// is using a fallback name.
TEST_P(WebAppPolicyManagerTest, ReinstallPlaceholderAppWithFallbackAppName) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value list(base::Value::Type::LIST);
  list.Append(GetFallbackAppNameItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetFallbackAppNameInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl));
  base::RunLoop().RunUntilIdle();

  auto reinstall_options = GetFallbackAppNameInstallOptions();
  reinstall_options.install_placeholder = false;
  reinstall_options.reinstall_placeholder = true;
  reinstall_options.wait_for_windows_closed = true;
  expected_options_list.push_back(std::move(reinstall_options));

  EXPECT_EQ(expected_options_list, install_options_list);
}

TEST_P(WebAppPolicyManagerTest, TryToInexistentPlaceholderApp) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  // Try to reinstall for app not installed by policy.
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kTabbedUrl));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_options_list, install_options_list);
}

TEST_P(WebAppPolicyManagerTest, SayRefreshTwoTimesQuickly) {
  if (ShouldSkipPWASpecificTest())
    return;
  policy_manager().Start();
  base::RunLoop().RunUntilIdle();
  // Add an app.
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetWindowedItem());
    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));
  }
  // Before it gets installed, set a policy that uninstalls it.
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetTabbedItem());
    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));
  }
  base::RunLoop().RunUntilIdle();

  // Both apps should have been installed.
  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());
  expected_options_list.push_back(GetTabbedInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);
  EXPECT_EQ(std::vector<GURL>({GURL(kWindowedUrl)}),
            externally_managed_app_manager().uninstall_requests());

  // There should be exactly 1 app remaining.
  std::map<AppId, GURL> apps = app_registrar().GetExternallyInstalledApps(
      ExternalInstallSource::kExternalPolicy);
  EXPECT_EQ(1u, apps.size());
  for (auto& it : apps)
    EXPECT_EQ(it.second, GURL(kTabbedUrl));
}

TEST_P(WebAppPolicyManagerTest, InstallResultHistogram) {
  if (ShouldSkipPWASpecificTest())
    return;
  base::HistogramTester histograms;
  policy_manager().Start();
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetWindowedItem());
    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 0);

    base::RunLoop().RunUntilIdle();

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 1);
    histograms.ExpectBucketCount(
        WebAppPolicyManager::kInstallResultHistogramName,
        InstallResultCode::kSuccessNewInstall, 1);
  }
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetTabbedItem());
    list.Append(GetNoContainerItem());
    SetInstallResultCode(
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown);

    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

    base::RunLoop().RunUntilIdle();
    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 3);
    histograms.ExpectBucketCount(
        WebAppPolicyManager::kInstallResultHistogramName,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 2);
  }
}

#if defined(OS_CHROMEOS)
TEST_P(WebAppPolicyManagerTest, DisableWebApps) {
  policy_manager().Start();
  base::RunLoop().RunUntilIdle();

  auto disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.empty());

  // Add camera to system features disable list policy.
  auto disabled_apps_list =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  disabled_apps_list->Append(policy::SystemFeature::kCamera);
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      std::move(disabled_apps_list));
  base::RunLoop().RunUntilIdle();

  std::set<SystemAppType> expected_disabled_apps;
  expected_disabled_apps.insert(SystemAppType::CAMERA);

  disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_EQ(disabled_apps, expected_disabled_apps);

  // Default disable mode is blocked.
  EXPECT_FALSE(policy_manager().IsDisabledAppsModeHidden());
  // Set disable mode to hidden.
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableMode,
      std::make_unique<base::Value>(policy::kHiddenDisableMode));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden());
}
#endif  // defined(OS_CHROMEOS)

TEST_P(WebAppPolicyManagerTest, WebAppSettingsDynamicRefresh) {
  if (ShouldSkipPWASpecificTest())
    return;
  const char kWebAppSettingInitialConfiguration[] = R"({
    "https://windowed.example/": {
      "run_on_os_login": "blocked"
    }
  })";

  MockWebAppPolicyManagerObserver mock_observer;
  policy_manager().AddObserver(&mock_observer);
  SetWebAppSettingsDictPref(kWebAppSettingInitialConfiguration);
  policy_manager().Start();
  AwaitPolicyManagerRefreshPolicySettings();
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(1, mock_observer.GetOnPolicyChangedCalledCount());

  SetWebAppSettingsDictPref(kWebAppSettingWithDefaultConfiguration);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(
      policy_manager().GetUrlRunOnOsLoginPolicy(GURL("http://foo.example")),
      RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(2, mock_observer.GetOnPolicyChangedCalledCount());
  policy_manager().RemoveObserver(&mock_observer);
}

TEST_P(WebAppPolicyManagerTest,
       WebAppSettingsApplyToExistingForceInstalledApp) {
  if (ShouldSkipPWASpecificTest())
    return;
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager().Start();
  AwaitPolicyManagerAppsSynchronized();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kAllowed);

  // Now apply WebSettings policy
  MockWebAppPolicyManagerObserver mock_observer;
  policy_manager().AddObserver(&mock_observer);
  SetWebAppSettingsDictPref(kWebAppSettingWithDefaultConfiguration);
  EXPECT_EQ(1, mock_observer.GetOnPolicyChangedCalledCount());
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(
      policy_manager().GetUrlRunOnOsLoginPolicy(GURL("http://foo.example")),
      RunOnOsLoginPolicy::kBlocked);
  policy_manager().RemoveObserver(&mock_observer);
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsForceInstallNewApps) {
  if (ShouldSkipPWASpecificTest())
    return;
  // Apply WebAppSettings Policy
  MockWebAppPolicyManagerObserver mock_observer;
  policy_manager().AddObserver(&mock_observer);
  SetWebAppSettingsDictPref(kWebAppSettingWithDefaultConfiguration);
  policy_manager().Start();
  AwaitPolicyManagerAppsSynchronized();
  EXPECT_EQ(1, mock_observer.GetOnPolicyChangedCalledCount());
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kWindowedUrl)),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kTabbedUrl)),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(policy_manager().GetUrlRunOnOsLoginPolicy(GURL(kNoContainerUrl)),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(
      policy_manager().GetUrlRunOnOsLoginPolicy(GURL("http://foo.example")),
      RunOnOsLoginPolicy::kBlocked);

  // Now add two sites, one that opens in a window and one that opens in a tab.
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  AwaitPolicyManagerAppsSynchronized();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list[0].run_on_os_login = true;
  expected_install_options_list.push_back(GetTabbedInstallOptions());
  expected_install_options_list[1].run_on_os_login = false;

  EXPECT_EQ(install_requests, expected_install_options_list);
  EXPECT_EQ(2, mock_observer.GetOnPolicyChangedCalledCount());
  policy_manager().RemoveObserver(&mock_observer);
}

INSTANTIATE_TEST_SUITE_P(WebAppPolicyManagerTestWithParams,
                         WebAppPolicyManagerTest,
                         testing::Values(
#if BUILDFLAG(IS_CHROMEOS_ASH)
                             TestParam::kLacrosDisabled,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                             TestParam::kLacrosEnabled));

}  // namespace web_app
