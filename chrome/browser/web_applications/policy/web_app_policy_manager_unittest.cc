// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

const char kWebAppSettingWithDefaultConfiguration[] = R"([
  {
    "manifest_id": "https://windowed.example/",
    "run_on_os_login": "run_windowed"
  },
  {
    "manifest_id": "https://tabbed.example/",
    "run_on_os_login": "allowed"
  },
  {
    "manifest_id": "https://no-container.example/",
    "run_on_os_login": "unsupported_value"
  },
  {
    "manifest_id": "bad.uri",
    "run_on_os_login": "allowed"
  },
  {
    "manifest_id": "*",
    "run_on_os_login": "blocked"
  }
])";

const char kDefaultFallbackAppName[] = "fallback app name";

constexpr char kWindowedUrl[] = "https://windowed.example/";
constexpr char kTabbedUrl[] = "https://tabbed.example/";
constexpr char kNoContainerUrl[] = "https://no-container.example/";

const char kDefaultCustomAppName[] = "custom app name";
constexpr char kDefaultCustomIconUrl[] = "https://windowed.example/icon.png";
constexpr char kUnsecureIconUrl[] = "http://windowed.example/icon.png";
constexpr char kDefaultCustomIconHash[] = "abcdef";

base::Value::Dict GetWindowedItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kWindowedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue);
}

ExternalInstallOptions GetWindowedInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kWindowedUrl),
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  return options;
}

base::Value::Dict GetTabbedItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kTabbedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerTabValue);
}

ExternalInstallOptions GetTabbedInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kTabbedUrl),
                                 mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  return options;
}

base::Value::Dict GetNoContainerItem() {
  return base::Value::Dict().Set(kUrlKey, kNoContainerUrl);
}

ExternalInstallOptions GetNoContainerInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kNoContainerUrl),
                                 mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  return options;
}

base::Value::Dict GetCreateDesktopShortcutDefaultItem() {
  return base::Value::Dict().Set(kUrlKey, kNoContainerUrl);
}

ExternalInstallOptions GetCreateDesktopShortcutDefaultInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kNoContainerUrl),
                                 mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  return options;
}

base::Value::Dict GetCreateDesktopShortcutFalseItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kNoContainerUrl)
      .Set(kCreateDesktopShortcutKey, false);
}

ExternalInstallOptions GetCreateDesktopShortcutFalseInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kNoContainerUrl),
                                 mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  return options;
}

base::Value::Dict GetCreateDesktopShortcutTrueItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kNoContainerUrl)
      .Set(kCreateDesktopShortcutKey, true);
}

ExternalInstallOptions GetCreateDesktopShortcutTrueInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kNoContainerUrl),
                                 mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  return options;
}

class MockAppRegistrarObserver : public WebAppRegistrarObserver {
 public:
  void OnWebAppSettingsPolicyChanged() override {
    on_policy_changed_call_count++;
  }

  int GetOnWebAppSettingsPolicyChangedCalledCount() const {
    return on_policy_changed_call_count;
  }

  void OnAppRegistrarDestroyed() override { NOTREACHED(); }

 private:
  int on_policy_changed_call_count = 0;
};

base::Value::Dict GetFallbackAppNameItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kWindowedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue)
      .Set(kFallbackAppNameKey, kDefaultFallbackAppName);
}

ExternalInstallOptions GetFallbackAppNameInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kWindowedUrl),
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  options.fallback_app_name = kDefaultFallbackAppName;
  return options;
}

base::Value::Dict GetCustomAppNameItem(std::string name) {
  return base::Value::Dict()
      .Set(kUrlKey, kWindowedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue)
      .Set(kCustomNameKey, std::move(name));
}

ExternalInstallOptions GetCustomAppNameInstallOptions(
    std::string name,
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kWindowedUrl),
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  options.override_name = std::move(name);
  return options;
}

base::Value::Dict GetCustomAppIconItem(bool secure = true) {
  return base::Value::Dict()
      .Set(kUrlKey, kWindowedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue)
      .Set(kCustomIconKey,
           base::Value::Dict()
               .Set(kCustomIconURLKey,
                    secure ? kDefaultCustomIconUrl : kUnsecureIconUrl)
               .Set(kCustomIconHashKey, kDefaultCustomIconHash));
}

ExternalInstallOptions GetCustomAppIconInstallOptions(
    bool wait_for_windows_closed = false) {
  ExternalInstallOptions options(GURL(kWindowedUrl),
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.wait_for_windows_closed = wait_for_windows_closed;
  options.override_icon_url = GURL(kDefaultCustomIconUrl);
  return options;
}

}  // namespace

enum class TestLacrosParam { kLacrosDisabled, kLacrosEnabled };

struct TestParam {
  TestLacrosParam lacros_params;
  bool prevent_close_enabled;
};

class WebAppPolicyManagerTest : public ChromeRenderViewHostTestHarness,
                                public testing::WithParamInterface<TestParam> {
 public:
  WebAppPolicyManagerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  WebAppPolicyManagerTest(const WebAppPolicyManagerTest&) = delete;
  WebAppPolicyManagerTest& operator=(const WebAppPolicyManagerTest&) = delete;
  ~WebAppPolicyManagerTest() override = default;

  void SetUp() override {
    BuildAndInitFeatureList();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Set up user manager to so that Lacros mode can be enabled.
    // Need to run the ChromeRenderViewHostTestHarness::SetUp() after the fake
    // user manager set up so that the scoped_user_manager can be destructed in
    // the correct order.
    // TODO(crbug.com/1463865): Consider setting up a fake user in all Ash web
    // app tests.
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    auto* user = fake_user_manager->AddUser(user_manager::StubAccountId());
    fake_user_manager->UserLoggedIn(user_manager::StubAccountId(),
                                    user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
#endif

    ChromeRenderViewHostTestHarness::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());

    auto fake_externally_managed_app_manager =
        std::make_unique<FakeExternallyManagedAppManager>(profile());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    test_system_app_manager_ =
        std::make_unique<ash::TestSystemWebAppManager>(profile());
#endif
    fake_externally_managed_app_manager_ =
        fake_externally_managed_app_manager.get();
    provider_->SetExternallyManagedAppManager(
        std::move(fake_externally_managed_app_manager));

    auto web_app_policy_manager =
        std::make_unique<WebAppPolicyManager>(profile());
    web_app_policy_manager_ = web_app_policy_manager.get();
    provider_->SetWebAppPolicyManager(std::move(web_app_policy_manager));

    fake_externally_managed_app_manager_->SetHandleInstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const ExternalInstallOptions& install_options) {
              const GURL& install_url = install_options.install_url;
              const AppId app_id = GenerateAppId(
                  /*manifest_id=*/absl::nullopt, install_url);
              if (app_registrar().GetAppById(app_id) &&
                  install_options.force_reinstall) {
                UnregisterApp(app_id);
              }
              if (!app_registrar().GetAppById(app_id)) {
                const auto install_source = install_options.install_source;
                std::unique_ptr<WebApp> web_app = test::CreateWebApp(
                    install_url,
                    ConvertExternalInstallSourceToSource(install_source));
                if (install_options.override_name) {
                  web_app->SetName(install_options.override_name.value());
                }
                RegisterApp(std::move(web_app));
                test::AddInstallUrlData(profile()->GetPrefs(), &sync_bridge(),
                                        app_id, install_url, install_source);
              }
              return ExternallyManagedAppManager::InstallResult(
                  install_result_code_, app_id);
            }));
    fake_externally_managed_app_manager_->SetHandleUninstallRequestCallback(
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    web_app_policy_manager_->SetSystemWebAppDelegateMap(
        &system_app_manager().system_app_delegates());
#endif

    test::AwaitStartWebAppProviderAndSubsystems(profile());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ASSERT_EQ(GetParam().lacros_params == TestLacrosParam::kLacrosEnabled,
              crosapi::browser_util::IsLacrosEnabled());
#endif
  }

  void TearDown() override {
    provider_->Shutdown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    test_system_app_manager_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SimulatePreviouslyInstalledApp(const GURL& url,
                                      ExternalInstallSource install_source) {
    auto web_app = test::CreateWebApp(
        url, ConvertExternalInstallSourceToSource(install_source));
    RegisterApp(std::move(web_app));
    test::AddInstallUrlData(profile()->GetPrefs(), &sync_bridge(),
                            GenerateAppId(/*manifest_id=*/absl::nullopt, url),
                            url, install_source);
  }

  void MakeInstalledAppPlaceholder(const GURL& url) {
    test::AddInstallUrlAndPlaceholderData(
        profile()->GetPrefs(), &sync_bridge(),
        GenerateAppId(/*manifest_id=*/absl::nullopt, url), url,
        ExternalInstallSource::kExternalPolicy, /*is_placeholder=*/true);
  }

 protected:
  void BuildAndInitFeatureList() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(
        features::kDesktopPWAsEnforceWebAppSettingsPolicy);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::vector<base::test::FeatureRef> lacros_flags =
        ash::standalone_browser::GetFeatureRefs();
    if (GetParam().lacros_params == TestLacrosParam::kLacrosEnabled) {
      base::Extend(enabled_features, lacros_flags);
    } else if (GetParam().lacros_params == TestLacrosParam::kLacrosDisabled) {
      base::Extend(disabled_features, lacros_flags);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    if (GetParam().prevent_close_enabled) {
      enabled_features.push_back(
          features::kDesktopPWAsEnforceWebAppSettingsPolicy);
      enabled_features.push_back(features::kDesktopPWAsPreventClose);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void InstallPwa(const std::string& url) {
    std::unique_ptr<WebAppInstallInfo> web_app_info =
        std::make_unique<WebAppInstallInfo>(
            GenerateManifestIdFromStartUrlOnly(GURL(url)));
    web_app_info->start_url = GURL(url);
    web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  bool ShouldSkipPWASpecificTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (GetParam().lacros_params == TestLacrosParam::kLacrosEnabled) {
      return true;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::TestSystemWebAppManager& system_app_manager() {
    return *test_system_app_manager_;
  }
#endif

  WebAppRegistrar& app_registrar() { return provider()->registrar_unsafe(); }
  WebAppSyncBridge& sync_bridge() { return provider()->sync_bridge_unsafe(); }
  WebAppPolicyManager& policy_manager() { return provider()->policy_manager(); }

  FakeExternallyManagedAppManager& externally_managed_app_manager() {
    return static_cast<FakeExternallyManagedAppManager&>(
        provider()->externally_managed_app_manager());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  ScopedTestingLocalState testing_local_state_;

  void SetWebAppSettingsListPref(const base::StringPiece pref) {
    ASSERT_OK_AND_ASSIGN(
        auto result,
        base::JSONReader::ReadAndReturnValueWithError(
            pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
    ASSERT_TRUE(result.is_list());
    profile()->GetPrefs()->Set(prefs::kWebAppSettings, std::move(result));
  }

  void ValidateEmptyWebAppSettingsPolicy() {
    EXPECT_TRUE(policy_manager().settings_by_url_.empty());
    ASSERT_TRUE(policy_manager().default_settings_);

    WebAppPolicyManager::WebAppSetting expected_default;
    EXPECT_EQ(policy_manager().default_settings_->run_on_os_login_policy,
              expected_default.run_on_os_login_policy);
  }

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  void UnregisterApp(const AppId& app_id) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->DeleteApp(app_id);
  }

  void SetInstallResultCode(webapps::InstallResultCode result_code) {
    install_result_code_ = result_code;
  }

  RunOnOsLoginPolicy GetUrlRunOnOsLoginPolicy(const std::string& manifest_id) {
    return policy_manager().GetUrlRunOnOsLoginPolicyByManifestId(manifest_id);
  }

  bool IsPreventCloseEnabled(const std::string& manifest_id) {
    return policy_manager().IsPreventCloseEnabled(
        web_app::GenerateAppIdFromManifestId(GURL(manifest_id)));
  }

  void WaitForAppsToSynchronize() {
    base::RunLoop loop;
    policy_manager().SetOnAppsSynchronizedCompletedCallbackForTesting(
        loop.QuitClosure());
    loop.Run();
  }

 private:
  webapps::InstallResultCode install_result_code_ =
      webapps::InstallResultCode::kSuccessNewInstall;

  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_;
  raw_ptr<FakeExternallyManagedAppManager, DanglingUntriaged>
      fake_externally_managed_app_manager_;
  raw_ptr<WebAppPolicyManager, DanglingUntriaged> web_app_policy_manager_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::TestSystemWebAppManager> test_system_app_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppPolicyManagerTest, NoPrefValues) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }

  const auto& install_requests =
      externally_managed_app_manager().install_requests();
  EXPECT_TRUE(install_requests.empty());
  ValidateEmptyWebAppSettingsPolicy();
}

TEST_P(WebAppPolicyManagerTest, NoForceInstalledApps) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 base::Value::List());

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();
  EXPECT_TRUE(install_requests.empty());
}

TEST_P(WebAppPolicyManagerTest, NoWebAppSettings) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  profile()->GetPrefs()->SetList(prefs::kWebAppSettings, base::Value::List());
  loop.Run();

  ValidateEmptyWebAppSettingsPolicy();
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsInvalidDefaultConfiguration) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  const char kWebAppSettingInvalidDefaultConfiguration[] = R"([
    {
      "manifest_id": "*",
      "run_on_os_login": "unsupported_value"
    }
  ])";

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(kWebAppSettingInvalidDefaultConfiguration);
  loop.Run();

  ValidateEmptyWebAppSettingsPolicy();
}

TEST_P(WebAppPolicyManagerTest,
       WebAppSettingsInvalidDefaultConfigurationWithValidAppPolicy) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  const char kWebAppSettingInvalidDefaultConfiguration[] = R"([
    {
      "manifest_id": "https://windowed.example/",
      "run_on_os_login": "run_windowed"
    },
    {
      "manifest_id": "*",
      "run_on_os_login": "unsupported_value"
    }
  ])";

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(kWebAppSettingInvalidDefaultConfiguration);
  loop.Run();

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kAllowed);
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsNoDefaultConfiguration) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  const char kWebAppSettingNoDefaultConfiguration[] = R"([
    {
      "manifest_id": "https://windowed.example/",
      "run_on_os_login": "run_windowed"
    },
    {
      "manifest_id": "https://tabbed.example/",
      "run_on_os_login": "blocked"
    },
    {
      "manifest_id": "https://no-container.example/",
      "run_on_os_login": "unsupported_value"
    },
    {
      "manifest_id": "bad.uri",
      "run_on_os_login": "allowed"
    }
  ])";

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(kWebAppSettingNoDefaultConfiguration);
  loop.Run();

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kAllowed);
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsWithDefaultConfiguration) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(kWebAppSettingWithDefaultConfiguration);
  loop.Run();

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kBlocked);
}

TEST_P(WebAppPolicyManagerTest, TwoForceInstalledApps) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value::List list;
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithNoDefaultLaunchContainer) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetNoContainerItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetNoContainerInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest,
       ForceInstallAppWithDefaultCreateDesktopShortcut) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetCreateDesktopShortcutDefaultItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(
      GetCreateDesktopShortcutDefaultInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithCreateDesktopShortcut) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetCreateDesktopShortcutFalseItem());
  list.Append(GetCreateDesktopShortcutTrueItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

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
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetFallbackAppNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetFallbackAppNameInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithCustomAppIcon) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetCustomAppIconItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetCustomAppIconInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

// If the custom icon URL is not https, the icon should be ignored.
TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithUnsecureCustomAppIcon) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetCustomAppIconItem(/*secure=*/false));
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetCustomAppIconInstallOptions());

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_FALSE(install_requests[0].override_icon_url);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithCustomAppName) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetCustomAppNameItem(kDefaultCustomAppName));
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(
      GetCustomAppNameInstallOptions(kDefaultCustomAppName));

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, ForceInstallAppWithCustomAppNameRefresh) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }

  std::string kPrefix = "Modified ";

  // Add app
  {
    base::Value::List list;
    list.Append(GetCustomAppNameItem(kDefaultCustomAppName));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }
  WaitForAppsToSynchronize();
  // Change custom name
  {
    base::Value::List list;
    list.Append(GetCustomAppNameItem(kPrefix + kDefaultCustomAppName));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }
  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  // App should have been installed twice, with a force-install the second time.
  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(
      GetCustomAppNameInstallOptions(kDefaultCustomAppName));
  auto options =
      GetCustomAppNameInstallOptions(kPrefix + kDefaultCustomAppName);
  options.force_reinstall = true;
  expected_install_options_list.push_back(options);

  EXPECT_EQ(install_requests, expected_install_options_list);

  base::flat_map<AppId, base::flat_set<GURL>> apps =
      app_registrar().GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ(kPrefix + kDefaultCustomAppName,
            app_registrar().GetAppShortName(apps.begin()->first));
}

TEST_P(WebAppPolicyManagerTest, DynamicRefresh) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List first_list;
  first_list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(first_list));

  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  base::Value::List second_list;
  second_list.Append(GetTabbedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(second_list));

  WaitForAppsToSynchronize();

  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_P(WebAppPolicyManagerTest, UninstallAppInstalledInPreviousSession) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }

  // Simulate two policy apps and a regular app that were installed in the
  // previous session.
  SimulatePreviouslyInstalledApp(GURL(kWindowedUrl),
                                 ExternalInstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(GURL(kTabbedUrl),
                                 ExternalInstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(GURL(kNoContainerUrl),
                                 ExternalInstallSource::kInternalDefault);

  // Push a policy with only one of the apps.
  base::Value::List first_list;
  first_list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(first_list));

  WaitForAppsToSynchronize();

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
  if (ShouldSkipPWASpecificTest()) {
    return;
  }

  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value::List first_list;
  first_list.Append(GetWindowedItem());
  first_list.Append(GetTabbedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(first_list));
  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  // Push a new policy without the tabbed site.
  base::Value::List second_list;
  second_list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(second_list));
  WaitForAppsToSynchronize();

  // We'll try to install the app again but ExternallyManagedAppManager will
  // handle not re-installing the app.
  expected_install_options_list.push_back(GetWindowedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  EXPECT_EQ(std::vector<GURL>({GURL(kTabbedUrl)}),
            externally_managed_app_manager().uninstall_requests());
}

// Tests that we correctly reinstall a placeholder app.
TEST_P(WebAppPolicyManagerTest, ReinstallPlaceholderAppSuccess) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  MakeInstalledAppPlaceholder(GURL(kWindowedUrl));
  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kSuccessNewInstall);

  auto reinstall_options =
      GetWindowedInstallOptions(/*wait_for_windows_closed=*/true);
  reinstall_options.install_placeholder = false;
  expected_options_list.push_back(std::move(reinstall_options));

  EXPECT_EQ(expected_options_list, install_options_list);
}

TEST_P(WebAppPolicyManagerTest, DoNotReinstallIfNotPlaceholder) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  // By default, the app being installed is not a placeholder app.
  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kFailedPlaceholderUninstall);

  // No other options are added to list as the app is currently not
  // installed as a placeholder app.
  EXPECT_EQ(expected_options_list, install_options_list);
}

// Tests that we correctly reinstall a placeholder app when the placeholder
// is using a fallback name.
TEST_P(WebAppPolicyManagerTest, ReinstallPlaceholderAppWithFallbackAppName) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetFallbackAppNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetFallbackAppNameInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  MakeInstalledAppPlaceholder(GURL(kWindowedUrl));
  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kSuccessNewInstall);

  auto reinstall_options =
      GetFallbackAppNameInstallOptions(/*wait_for_windows_closed=*/true);
  reinstall_options.install_placeholder = false;
  expected_options_list.push_back(std::move(reinstall_options));

  EXPECT_EQ(expected_options_list, install_options_list);
}

TEST_P(WebAppPolicyManagerTest, TryToInexistentPlaceholderApp) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::Value::List list;
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());

  const auto& install_options_list =
      externally_managed_app_manager().install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  // Try to reinstall for app not installed by policy.
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kTabbedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kFailedPlaceholderUninstall);

  EXPECT_EQ(expected_options_list, install_options_list);
}

TEST_P(WebAppPolicyManagerTest, SayRefreshTwoTimesQuickly) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  // Add an app.
  {
    base::Value::List list;
    list.Append(GetWindowedItem());
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }
  // Before it gets installed, set a policy that uninstalls it.
  {
    base::Value::List list;
    list.Append(GetTabbedItem());
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }

  // `OnAppsSynchronized` should be triggered twice.
  WaitForAppsToSynchronize();
  WaitForAppsToSynchronize();

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
  base::flat_map<AppId, base::flat_set<GURL>> apps =
      app_registrar().GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  EXPECT_EQ(1u, apps.size());
  for (auto& it : apps) {
    EXPECT_EQ(*it.second.begin(), GURL(kTabbedUrl));
  }
}

TEST_P(WebAppPolicyManagerTest, InstallResultHistogram) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  base::HistogramTester histograms;
  {
    base::Value::List list;
    list.Append(GetWindowedItem());
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 0);

    WaitForAppsToSynchronize();

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 1);
    histograms.ExpectBucketCount(
        WebAppPolicyManager::kInstallResultHistogramName,
        webapps::InstallResultCode::kSuccessNewInstall, 1);
  }
  {
    base::Value::List list;
    list.Append(GetTabbedItem());
    list.Append(GetNoContainerItem());
    SetInstallResultCode(
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);

    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));

    WaitForAppsToSynchronize();

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 3);
    histograms.ExpectBucketCount(
        WebAppPolicyManager::kInstallResultHistogramName,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 2);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(WebAppPolicyManagerTest, DisableSystemWebApps) {
  auto disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.empty());

  // Add supported system web apps to system features disable list policy.
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List()
          .Append(static_cast<int>(policy::SystemFeature::kCamera))
          .Append(static_cast<int>(policy::SystemFeature::kOsSettings))
          .Append(static_cast<int>(policy::SystemFeature::kScanning))
          .Append(static_cast<int>(policy::SystemFeature::kExplore))
          .Append(static_cast<int>(policy::SystemFeature::kCrosh))
          .Append(static_cast<int>(policy::SystemFeature::kTerminal))
          .Append(static_cast<int>(policy::SystemFeature::kGallery)));
  base::RunLoop().RunUntilIdle();

  const std::set<ash::SystemWebAppType> expected_disabled_apps{
      ash::SystemWebAppType::CAMERA,   ash::SystemWebAppType::SETTINGS,
      ash::SystemWebAppType::SCANNING, ash::SystemWebAppType::HELP,
      ash::SystemWebAppType::CROSH,    ash::SystemWebAppType::TERMINAL,
      ash::SystemWebAppType::MEDIA};

  disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_EQ(disabled_apps, expected_disabled_apps);

  // Default disable mode is blocked.
  EXPECT_FALSE(policy_manager().IsDisabledAppsModeHidden());
  // Set disable mode to hidden.
  testing_local_state_.Get()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableMode,
      base::Value(policy::kHiddenDisableMode));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_P(WebAppPolicyManagerTest, WebAppSettingsDynamicRefresh) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  const char kWebAppSettingInitialConfiguration[] = R"([
    {
      "manifest_id": "https://windowed.example/",
      "run_on_os_login": "blocked"
    }
  ])";

  MockAppRegistrarObserver mock_observer;
  app_registrar().AddObserver(&mock_observer);

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(kWebAppSettingInitialConfiguration);
  loop.Run();

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(1, mock_observer.GetOnWebAppSettingsPolicyChangedCalledCount());

  SetWebAppSettingsListPref(kWebAppSettingWithDefaultConfiguration);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(2, mock_observer.GetOnWebAppSettingsPolicyChangedCalledCount());
  app_registrar().RemoveObserver(&mock_observer);
}

TEST_P(WebAppPolicyManagerTest,
       WebAppSettingsApplyToExistingForceInstalledApp) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value::List list;
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());

  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));
  WaitForAppsToSynchronize();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);

  // Now apply WebSettings policy
  SetWebAppSettingsListPref(kWebAppSettingWithDefaultConfiguration);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kBlocked);
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsForceInstallNewApps) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  // Apply WebAppSettings Policy
  MockAppRegistrarObserver mock_observer;
  app_registrar().AddObserver(&mock_observer);

  base::RunLoop settings_loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      settings_loop.QuitClosure());
  SetWebAppSettingsListPref(kWebAppSettingWithDefaultConfiguration);
  settings_loop.Run();

  EXPECT_EQ(1, mock_observer.GetOnWebAppSettingsPolicyChangedCalledCount());
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kBlocked);

  // Now add two sites, one that opens in a window and one that opens in a tab.
  base::Value::List list;
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());

  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));
  WaitForAppsToSynchronize();

  provider()->command_manager().AwaitAllCommandsCompleteForTesting();

  const auto& install_requests =
      externally_managed_app_manager().install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
  EXPECT_EQ(2, mock_observer.GetOnWebAppSettingsPolicyChangedCalledCount());
  app_registrar().RemoveObserver(&mock_observer);
}

TEST_P(WebAppPolicyManagerTest, WebAppSettingsPreventClose) {
  if (ShouldSkipPWASpecificTest()) {
    return;
  }
  const char kWebAppSettingNoDefaultConfiguration[] = R"([
    {
      "manifest_id": "https://windowed.example/",
      "run_on_os_login": "run_windowed",
      "prevent_close_after_run_on_os_login": true
    },
    {
      "manifest_id": "https://tabbed.example/",
      "run_on_os_login": "blocked",
      "prevent_close_after_run_on_os_login": true
    },
    {
      "manifest_id": "https://no-container.example/",
      "run_on_os_login": "unsupported_value",
      "prevent_close_after_run_on_os_login": true
    },
    {
      "manifest_id": "bad.uri",
      "run_on_os_login": "allowed",
      "prevent_close_after_run_on_os_login": true
    }
  ])";

  // Make sure that WebAppRegistrar::GetComputedManifestId does not fail.
  InstallPwa(kWindowedUrl);
  InstallPwa(kTabbedUrl);
  InstallPwa(kNoContainerUrl);

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(kWebAppSettingNoDefaultConfiguration);
  loop.Run();

#if BUILDFLAG(IS_CHROMEOS)
  if (GetParam().prevent_close_enabled) {
    EXPECT_TRUE(IsPreventCloseEnabled(kWindowedUrl));
    EXPECT_FALSE(IsPreventCloseEnabled(kTabbedUrl));
    EXPECT_FALSE(IsPreventCloseEnabled(kNoContainerUrl));
  } else {
    EXPECT_FALSE(IsPreventCloseEnabled(kWindowedUrl));
    EXPECT_FALSE(IsPreventCloseEnabled(kTabbedUrl));
    EXPECT_FALSE(IsPreventCloseEnabled(kNoContainerUrl));
  }
#else
  EXPECT_FALSE(IsPreventCloseEnabled(kWindowedUrl));
  EXPECT_FALSE(IsPreventCloseEnabled(kTabbedUrl));
  EXPECT_FALSE(IsPreventCloseEnabled(kNoContainerUrl));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

INSTANTIATE_TEST_SUITE_P(WebAppPolicyManagerTestWithParams,
                         WebAppPolicyManagerTest,
                         testing::Values(
#if BUILDFLAG(IS_CHROMEOS_ASH)
                             TestParam({TestLacrosParam::kLacrosDisabled,
                                        /*prevent_close_enabled=*/false}),
                             TestParam({TestLacrosParam::kLacrosDisabled,
                                        /*prevent_close_enabled=*/true}),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                             TestParam({TestLacrosParam::kLacrosEnabled,
                                        /*prevent_close_enabled=*/false}),
                             TestParam({TestLacrosParam::kLacrosEnabled,
                                        /*prevent_close_enabled=*/true})),
                         [](const ::testing::TestParamInfo<TestParam>& info) {
                           std::string test_name = "Test_";
                           if (info.param.lacros_params ==
                               TestLacrosParam::kLacrosEnabled)
                             test_name.append("LacrosEnabled_");
                           else
                             test_name.append("LacrosDisabled_");

                           if (info.param.prevent_close_enabled) {
                             test_name.append("PreventCloseEnabled");
                           } else {
                             test_name.append("PreventCloseDisabled");
                           }
                           return test_name;
                         });

}  // namespace web_app
