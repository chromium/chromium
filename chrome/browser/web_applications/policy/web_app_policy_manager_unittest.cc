// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <memory>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/system_features_disable_list_constants.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif  // BUILDFLAG(IS_WIN)

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

base::Value::Dict GetTabbedItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kTabbedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerTabValue);
}

base::Value::Dict GetNoContainerItem() {
  return base::Value::Dict().Set(kUrlKey, kNoContainerUrl);
}

base::Value::Dict GetCreateDesktopShortcutFalseItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kNoContainerUrl)
      .Set(kCreateDesktopShortcutKey, false);
}

base::Value::Dict GetCreateDesktopShortcutTrueItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kNoContainerUrl)
      .Set(kCreateDesktopShortcutKey, true);
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
  MOCK_METHOD(void, OnWebAppsDisabledModeChanged, (), (override));

 private:
  int on_policy_changed_call_count = 0;
};

base::Value::Dict GetFallbackAppNameItem() {
  return base::Value::Dict()
      .Set(kUrlKey, kWindowedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue)
      .Set(kFallbackAppNameKey, kDefaultFallbackAppName);
}

base::Value::Dict GetCustomAppNameItem(std::string name) {
  return base::Value::Dict()
      .Set(kUrlKey, kWindowedUrl)
      .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue)
      .Set(kCustomNameKey, std::move(name));
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

void SetWebAppSettingsListPref(Profile* profile, std::string_view pref) {
  ASSERT_OK_AND_ASSIGN(
      auto result,
      base::JSONReader::ReadAndReturnValueWithError(
          pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(result.is_list());
  profile->GetPrefs()->Set(prefs::kWebAppSettings, std::move(result));
}

void SetWebAppInstallForceListPref(Profile* profile, std::string_view pref) {
  ASSERT_OK_AND_ASSIGN(
      auto result,
      base::JSONReader::ReadAndReturnValueWithError(
          pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(result.is_list());
  profile->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(result));
}

}  // namespace

class WebAppPolicyManagerTestBase : public WebAppTest {
 public:
  WebAppPolicyManagerTestBase() = default;
  WebAppPolicyManagerTestBase(const WebAppPolicyManagerTestBase&) = delete;
  WebAppPolicyManagerTestBase& operator=(const WebAppPolicyManagerTestBase&) =
      delete;
  ~WebAppPolicyManagerTestBase() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    // Need to run the WebAppTest::SetUp() after the fake
    // user manager set up so that the scoped_user_manager can be destructed in
    // the correct order.
    // TODO(crbug.com/40275387): Consider setting up a fake user in all Ash web
    // app tests.
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    fake_user_manager->AddUser(user_manager::StubAccountId());
    fake_user_manager->UserLoggedIn(
        user_manager::StubAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(
            user_manager::StubAccountId()));
#endif

    WebAppTest::SetUp();
#if BUILDFLAG(IS_CHROMEOS)
    test_system_app_manager_ =
        std::make_unique<ash::TestSystemWebAppManager>(profile());
#endif
    auto web_app_policy_manager =
        std::make_unique<WebAppPolicyManager>(profile());
#if BUILDFLAG(IS_CHROMEOS)
    web_app_policy_manager->SetSystemWebAppDelegateMap(
        &system_app_manager().system_app_delegates());
#endif
    fake_provider().SetWebAppPolicyManager(std::move(web_app_policy_manager));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS)
    test_system_app_manager_.reset();
#endif
    WebAppTest::TearDown();
  }

  void SimulatePreviouslyInstalledApp(const GURL& url,
                                      ExternalInstallSource install_source) {
    auto web_app = test::CreateWebApp(
        url, ConvertExternalInstallSourceToSource(install_source));
    RegisterApp(std::move(web_app));
    test::AddInstallUrlData(
        profile()->GetPrefs(), &sync_bridge(),
        GenerateAppId(/*manifest_id_path=*/std::nullopt, url), url,
        install_source);
  }

  void MakeInstalledAppPlaceholder(const GURL& url) {
    test::AddInstallUrlAndPlaceholderData(
        profile()->GetPrefs(), &sync_bridge(),
        GenerateAppId(/*manifest_id_path=*/std::nullopt, url), url,
        ExternalInstallSource::kExternalPolicy, /*is_placeholder=*/true);
  }

 protected:
  void InstallPwa(const std::string& url) {
    std::unique_ptr<WebAppInstallInfo> web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(url));
    web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

#if BUILDFLAG(IS_CHROMEOS)
  ash::TestSystemWebAppManager& system_app_manager() {
    return *test_system_app_manager_;
  }
#endif

  WebAppRegistrar& app_registrar() {
    return fake_provider().registrar_unsafe();
  }
  WebAppSyncBridge& sync_bridge() {
    return fake_provider().sync_bridge_unsafe();
  }
  WebAppPolicyManager& policy_manager() {
    return fake_provider().policy_manager();
  }

  void ValidateEmptyWebAppSettingsPolicy() {
    EXPECT_TRUE(policy_manager().settings_by_url_.empty());

    WebAppPolicyManager::WebAppSetting expected_default;
    EXPECT_EQ(policy_manager().default_settings_.run_on_os_login_policy,
              expected_default.run_on_os_login_policy);
  }

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  void UnregisterApp(const webapps::AppId& app_id) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->DeleteApp(app_id);
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

  const WebApp* GetPolicyInstalledWindowedApp() {
    return app_registrar().LookUpAppByInstallSourceInstallUrl(
        WebAppManagement::kPolicy, GURL(kWindowedUrl));
  }

  const WebApp* GetPolicyInstalledTabbedApp() {
    return app_registrar().LookUpAppByInstallSourceInstallUrl(
        WebAppManagement::kPolicy, GURL(kTabbedUrl));
  }

  const WebApp* GetPolicyInstalledNoContainerApp() {
    return app_registrar().LookUpAppByInstallSourceInstallUrl(
        WebAppManagement::kPolicy, GURL(kNoContainerUrl));
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  data_decoder::test::InProcessDataDecoder data_decoder_;

 private:
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::TestSystemWebAppManager> test_system_app_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
};

class WebAppPolicyManagerTest : public WebAppPolicyManagerTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  using InstallResults = std::map<GURL /*install_url*/,
                                  ExternallyManagedAppManager::InstallResult>;
  using UninstallResults =
      std::map<GURL /*install_url*/, webapps::UninstallResultCode>;
  using SynchronizeFuture =
      base::test::TestFuture<InstallResults, UninstallResults>;

  WebAppPolicyManagerTest() = default;
  WebAppPolicyManagerTest(const WebAppPolicyManagerTest&) = delete;
  WebAppPolicyManagerTest& operator=(const WebAppPolicyManagerTest&) = delete;
  ~WebAppPolicyManagerTest() override = default;
};

TEST_F(WebAppPolicyManagerTest, GetPolicyIdsForWebApp) {
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager())
          .CreateBasicInstallPageState(kInstallUrl, kManifestUrl, kWebAppUrl);

  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);

  SynchronizeFuture result;
  std::vector<ExternalInstallOptions> install_options_list;
  install_options_list.emplace_back(kInstallUrl,
                                    /*user_display_mode=*/std::nullopt,
                                    ExternalInstallSource::kExternalPolicy);

  fake_provider().externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
      result.GetCallback());
  ASSERT_TRUE(result.Wait());
  const WebApp* app = fake_provider().registrar_unsafe().GetAppById(app_id);

  EXPECT_EQ(
      WebAppPolicyManager::GetPolicyIds(profile(), *app),
      std::vector<std::string>({"https://www.example.com/install_url.html"}));
}

TEST_F(WebAppPolicyManagerTest, GetPolicyIdsForIsolatedWebApp) {
  auto bundle = IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
                    .BuildBundle();
  IsolatedWebAppUrlInfo info =
      bundle
          ->InstallWithSource(profile(),
                              &IsolatedWebAppInstallSource::FromExternalPolicy)
          .value();
  const WebApp* app =
      fake_provider().registrar_unsafe().GetAppById(info.app_id());

  EXPECT_EQ(WebAppPolicyManager::GetPolicyIds(profile(), *app),
            std::vector<std::string>({info.web_bundle_id().id()}));
}

TEST_F(WebAppPolicyManagerTest, NoPrefValues) {
  ValidateEmptyWebAppSettingsPolicy();
}

TEST_F(WebAppPolicyManagerTest, NoForceInstalledApps) {
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 base::Value::List());

  WaitForAppsToSynchronize();
  ASSERT_TRUE(app_registrar().is_empty());
}

TEST_F(WebAppPolicyManagerTest, NoWebAppSettings) {
  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  profile()->GetPrefs()->SetList(prefs::kWebAppSettings, base::Value::List());
  loop.Run();

  ValidateEmptyWebAppSettingsPolicy();
}

TEST_F(WebAppPolicyManagerTest, WebAppSettingsInvalidDefaultConfiguration) {
  const char kWebAppSettingInvalidDefaultConfiguration[] = R"([
    {
      "manifest_id": "*",
      "run_on_os_login": "unsupported_value"
    }
  ])";

  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(profile(),
                            kWebAppSettingInvalidDefaultConfiguration);
  loop.Run();

  ValidateEmptyWebAppSettingsPolicy();
}

TEST_F(WebAppPolicyManagerTest,
       WebAppSettingsInvalidDefaultConfigurationWithValidAppPolicy) {
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
  SetWebAppSettingsListPref(profile(),
                            kWebAppSettingInvalidDefaultConfiguration);
  loop.Run();

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kAllowed);
}

TEST_F(WebAppPolicyManagerTest, WebAppSettingsNoDefaultConfiguration) {
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
  SetWebAppSettingsListPref(profile(), kWebAppSettingNoDefaultConfiguration);
  loop.Run();

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kAllowed);
}

TEST_F(WebAppPolicyManagerTest, WebAppSettingsWithDefaultConfiguration) {
  base::RunLoop loop;
  policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
      loop.QuitClosure());
  SetWebAppSettingsListPref(profile(), kWebAppSettingWithDefaultConfiguration);
  loop.Run();

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kBlocked);
}

TEST_F(WebAppPolicyManagerTest, TwoForceInstalledApps) {
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value::List list;
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_NE(GetPolicyInstalledTabbedApp(), nullptr);
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithNoDefaultLaunchContainer) {
  base::Value::List list;
  list.Append(GetNoContainerItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledNoContainerApp(), nullptr);
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithCreateDesktopShortcut) {
  base::Value::List list;
  list.Append(GetCreateDesktopShortcutFalseItem());
  list.Append(GetCreateDesktopShortcutTrueItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledNoContainerApp(), nullptr);
  EXPECT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
            app_registrar().GetInstallState(
                GetPolicyInstalledNoContainerApp()->app_id()));
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithFallbackAppName) {
  base::Value::List list;
  list.Append(GetFallbackAppNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_EQ(kDefaultFallbackAppName,
            app_registrar().GetAppShortName(
                GetPolicyInstalledWindowedApp()->app_id()));
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithCustomAppIcon) {
  base::Value::List list;
  list.Append(GetCustomAppIconItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();
  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
}

// If the custom icon URL is not https, the icon should be ignored.
TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithUnsecureCustomAppIcon) {
  base::Value::List list;
  list.Append(GetCustomAppIconItem(/*secure=*/false));
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  const webapps::AppId& app_id = GetPolicyInstalledWindowedApp()->app_id();
  auto icon_infos = app_registrar().GetAppIconInfos(app_id);
  EXPECT_EQ(0u, icon_infos.size());
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithCustomAppName) {
  base::Value::List list;
  list.Append(GetCustomAppNameItem(kDefaultCustomAppName));
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_EQ(kDefaultCustomAppName,
            app_registrar().GetAppShortName(
                GetPolicyInstalledWindowedApp()->app_id()));
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithCustomAppNameRefresh) {
  std::string kPrefix = "Modified ";

  // Add app
  {
    base::Value::List list;
    list.Append(GetCustomAppNameItem(kDefaultCustomAppName));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }
  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_EQ(kDefaultCustomAppName,
            app_registrar().GetAppShortName(
                GetPolicyInstalledWindowedApp()->app_id()));

  // Change custom name
  {
    base::Value::List list;
    list.Append(GetCustomAppNameItem(kPrefix + kDefaultCustomAppName));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }
  WaitForAppsToSynchronize();

  base::flat_map<webapps::AppId, base::flat_set<GURL>> apps =
      app_registrar().GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ(kPrefix + kDefaultCustomAppName,
            app_registrar().GetAppShortName(apps.begin()->first));
}

TEST_F(WebAppPolicyManagerTest, DynamicRefresh) {
  base::Value::List first_list;
  first_list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(first_list));

  WaitForAppsToSynchronize();
  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);

  base::Value::List second_list;
  second_list.Append(GetTabbedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(second_list));

  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledTabbedApp(), nullptr);
}

TEST_F(WebAppPolicyManagerTest, UninstallAppInstalledInPreviousSession) {
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

  // Verify only the app corresponding to `kWindowedUrl` is installed,
  // everything else is removed.
  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_EQ(GetPolicyInstalledNoContainerApp(), nullptr);
  EXPECT_EQ(GetPolicyInstalledTabbedApp(), nullptr);
}

// Tests that we correctly uninstall an app that we installed in the same
// session.
TEST_F(WebAppPolicyManagerTest, UninstallAppInstalledInCurrentSession) {
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value::List first_list;
  first_list.Append(GetWindowedItem());
  first_list.Append(GetTabbedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(first_list));
  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_NE(GetPolicyInstalledTabbedApp(), nullptr);

  // Push a new policy without the tabbed site.
  base::Value::List second_list;
  second_list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(second_list));
  WaitForAppsToSynchronize();

  // GetPolicyInstalledTabbedApp() will be deleted.
  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_EQ(GetPolicyInstalledTabbedApp(), nullptr);
}

// Tests that we correctly reinstall a placeholder app.
TEST_F(WebAppPolicyManagerTest, ReinstallPlaceholderAppSuccess) {
  base::Value::List list;
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();
  ASSERT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  const webapps::AppId& app_id = GetPolicyInstalledWindowedApp()->app_id();
  EXPECT_TRUE(
      app_registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy));

  web_contents_manager().CreateBasicInstallPageState(
      GURL(kWindowedUrl), GURL(kWindowedUrl), GURL(kWindowedUrl));
  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_FALSE(
      app_registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy));
}

TEST_F(WebAppPolicyManagerTest, DoNotReinstallIfNotPlaceholder) {
  base::Value::List list;
  list.Append(GetWindowedItem());
  web_contents_manager().CreateBasicInstallPageState(
      GURL(kWindowedUrl), GURL(kWindowedUrl), GURL(kWindowedUrl));
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));
  WaitForAppsToSynchronize();

  ASSERT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  const webapps::AppId& app_id = GetPolicyInstalledWindowedApp()->app_id();
  EXPECT_FALSE(
      app_registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy));

  // By default, the app being installed is not a placeholder app.
  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kFailedPlaceholderUninstall);

  // App is still currently not installed as a placeholder app.
  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_FALSE(
      app_registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy));
}

// Tests that we correctly reinstall a placeholder app when the placeholder
// is using a fallback name.
TEST_F(WebAppPolicyManagerTest, ReinstallPlaceholderAppWithFallbackAppName) {
  base::Value::List list;
  list.Append(GetFallbackAppNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  ASSERT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  const webapps::AppId& app_id = GetPolicyInstalledWindowedApp()->app_id();
  EXPECT_TRUE(
      app_registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(kDefaultFallbackAppName, app_registrar().GetAppShortName(app_id));

  web_contents_manager().CreateBasicInstallPageState(
      GURL(kWindowedUrl), GURL(kWindowedUrl), GURL(kWindowedUrl));
  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kWindowedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_FALSE(
      app_registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy));
  EXPECT_NE(kDefaultFallbackAppName, app_registrar().GetAppShortName(app_id));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TryToInstallNonexistentPlaceholderApp \
  DISABLED_TryToInstallNonexistentPlaceholderApp
#else
#define MAYBE_TryToInstallNonexistentPlaceholderApp \
  TryToInstallNonexistentPlaceholderApp
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(WebAppPolicyManagerTest, MAYBE_TryToInstallNonexistentPlaceholderApp) {
  base::Value::List list;
  list.Append(GetWindowedItem());
  web_contents_manager().CreateBasicInstallPageState(
      GURL(kWindowedUrl), GURL(kWindowedUrl), GURL(kWindowedUrl));
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  WaitForAppsToSynchronize();

  ASSERT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_FALSE(app_registrar().IsPlaceholderApp(
      GetPolicyInstalledWindowedApp()->app_id(), WebAppManagement::kPolicy));

  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      future;
  // Try to reinstall for app not installed by policy.
  policy_manager().ReinstallPlaceholderAppIfNecessary(GURL(kTabbedUrl),
                                                      future.GetCallback());
  EXPECT_EQ(future.Get<1>().code,
            webapps::InstallResultCode::kFailedPlaceholderUninstall);

  ASSERT_EQ(GetPolicyInstalledTabbedApp(), nullptr);
}

TEST_F(WebAppPolicyManagerTest, SayRefreshTwoTimesQuickly) {
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

  // There should be exactly 1 app remaining at the end for the `tabbed` item.
  base::flat_map<webapps::AppId, base::flat_set<GURL>> apps =
      app_registrar().GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  EXPECT_EQ(1u, apps.size());
  for (auto& it : apps) {
    EXPECT_EQ(*it.second.begin(), GURL(kTabbedUrl));
  }
}

TEST_F(WebAppPolicyManagerTest, InstallResultHistogram) {
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

    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));

    WaitForAppsToSynchronize();

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 3);
    histograms.ExpectBucketCount(
        WebAppPolicyManager::kInstallResultHistogramName,
        webapps::InstallResultCode::kSuccessNewInstall, 3);
  }
}

TEST_F(WebAppPolicyManagerTest, InvalidUrlParsingSkipped) {
  base::Value::Dict invalid_url_policy =
      base::Value::Dict()
          .Set(kUrlKey, "abcdef")
          .Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue);
  base::Value::List policy_list;
  policy_list.Append(std::move(invalid_url_policy));
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(policy_list));

  WaitForAppsToSynchronize();

  // No apps are installed.
  ASSERT_TRUE(app_registrar().is_empty());
}

TEST_F(WebAppPolicyManagerTest, WebAppSettingsDynamicRefresh) {
  const char kWebAppSettingInitialConfiguration[] = R"([
    {
      "manifest_id": "https://windowed.example/",
      "run_on_os_login": "blocked"
    }
  ])";

  MockAppRegistrarObserver mock_observer;
  app_registrar().AddObserver(&mock_observer);

  {
    base::RunLoop loop;
    policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
        loop.QuitClosure());
    SetWebAppSettingsListPref(profile(), kWebAppSettingInitialConfiguration);
    loop.Run();
  }

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(1, mock_observer.GetOnWebAppSettingsPolicyChangedCalledCount());

  {
    base::RunLoop loop;
    policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
        loop.QuitClosure());
    SetWebAppSettingsListPref(profile(),
                              kWebAppSettingWithDefaultConfiguration);
    loop.Run();
  }
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

TEST_F(WebAppPolicyManagerTest,
       WebAppSettingsApplyToExistingForceInstalledApp) {
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value::List list;
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());

  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));
  WaitForAppsToSynchronize();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_NE(GetPolicyInstalledTabbedApp(), nullptr);

  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kAllowed);

  // Now apply WebSettings policy
  SetWebAppSettingsListPref(profile(), kWebAppSettingWithDefaultConfiguration);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kBlocked);
}

TEST_F(WebAppPolicyManagerTest, WebAppSettingsForceInstallNewApps) {
  // Apply WebAppSettings Policy
  MockAppRegistrarObserver mock_observer;
  app_registrar().AddObserver(&mock_observer);

  {
    base::RunLoop settings_loop;
    policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
        settings_loop.QuitClosure());
    SetWebAppSettingsListPref(profile(),
                              kWebAppSettingWithDefaultConfiguration);
    settings_loop.Run();
  }

  EXPECT_EQ(1, mock_observer.GetOnWebAppSettingsPolicyChangedCalledCount());
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kWindowedUrl),
            RunOnOsLoginPolicy::kRunWindowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kTabbedUrl), RunOnOsLoginPolicy::kAllowed);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy(kNoContainerUrl),
            RunOnOsLoginPolicy::kBlocked);
  EXPECT_EQ(GetUrlRunOnOsLoginPolicy("http://foo.example"),
            RunOnOsLoginPolicy::kBlocked);

  {
    base::RunLoop loop;
    policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
        loop.QuitClosure());
    // Now add two sites, one that opens in a window and one that opens in a
    // tab.
    profile()->GetPrefs()->SetList(
        prefs::kWebAppInstallForceList,
        base::Value::List().Append(GetWindowedItem()).Append(GetTabbedItem()));
    loop.Run();
  }

  fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_NE(GetPolicyInstalledWindowedApp(), nullptr);
  EXPECT_NE(GetPolicyInstalledTabbedApp(), nullptr);
  EXPECT_EQ(2, mock_observer.GetOnWebAppSettingsPolicyChangedCalledCount());
  app_registrar().RemoveObserver(&mock_observer);
}

#if BUILDFLAG(IS_CHROMEOS)

class WebAppPolicyManagerDisableListTest : public WebAppPolicyManagerTestBase {
 public:
  WebAppPolicyManagerDisableListTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kSystemFeaturesDisableListHidden);
  }

  WebAppPolicyManagerDisableListTest(
      const WebAppPolicyManagerDisableListTest&) = delete;
  WebAppPolicyManagerDisableListTest& operator=(
      const WebAppPolicyManagerDisableListTest&) = delete;

  ~WebAppPolicyManagerDisableListTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebAppPolicyManagerDisableListTest, DisableSystemWebApps) {
  auto disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.empty());

  // Add supported system web apps to system features disable list policy.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List()
          .Append(static_cast<int>(policy::SystemFeature::kCamera))
          .Append(static_cast<int>(policy::SystemFeature::kOsSettings))
          .Append(static_cast<int>(policy::SystemFeature::kScanning))
          .Append(static_cast<int>(policy::SystemFeature::kExplore))
          .Append(static_cast<int>(policy::SystemFeature::kCrosh))
          .Append(static_cast<int>(policy::SystemFeature::kTerminal))
          .Append(static_cast<int>(policy::SystemFeature::kGallery))
          .Append(static_cast<int>(policy::SystemFeature::kPrintJobs))
          .Append(static_cast<int>(policy::SystemFeature::kKeyShortcuts))
          .Append(static_cast<int>(policy::SystemFeature::kRecorder)));
  base::RunLoop().RunUntilIdle();

  disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_THAT(
      disabled_apps,
      testing::UnorderedElementsAre(
          ash::SystemWebAppType::CAMERA, ash::SystemWebAppType::SETTINGS,
          ash::SystemWebAppType::SCANNING, ash::SystemWebAppType::HELP,
          ash::SystemWebAppType::CROSH, ash::SystemWebAppType::TERMINAL,
          ash::SystemWebAppType::MEDIA, ash::SystemWebAppType::PRINT_MANAGEMENT,
          ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION,
          ash::SystemWebAppType::RECORDER, ash::SystemWebAppType::GRADUATION,
          ash::SystemWebAppType::BOCA));

  // If the app is disabled by the SystemFeaturesDisableList policy, default
  // disable mode for user sessions is hidden.
  EXPECT_TRUE(
      policy_manager().IsDisabledAppsModeHidden(ash::SystemWebAppType::CAMERA));
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden(
      ash::SystemWebAppType::SETTINGS));
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden(
      ash::SystemWebAppType::SCANNING));
  EXPECT_TRUE(
      policy_manager().IsDisabledAppsModeHidden(ash::SystemWebAppType::HELP));
  EXPECT_TRUE(
      policy_manager().IsDisabledAppsModeHidden(ash::SystemWebAppType::CROSH));
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden(
      ash::SystemWebAppType::TERMINAL));
  EXPECT_TRUE(
      policy_manager().IsDisabledAppsModeHidden(ash::SystemWebAppType::MEDIA));
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden(
      ash::SystemWebAppType::PRINT_MANAGEMENT));
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden(
      ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION));
  EXPECT_TRUE(policy_manager().IsDisabledAppsModeHidden(
      ash::SystemWebAppType::RECORDER));

  // For apps not hidden by the SystemFeaturesDisableList policy, default
  // disable mode for user sessions is blocked.
  EXPECT_FALSE(policy_manager().IsDisabledAppsModeHidden(
      ash::SystemWebAppType::GRADUATION));
  EXPECT_FALSE(
      policy_manager().IsDisabledAppsModeHidden(ash::SystemWebAppType::BOCA));
}

class WebAppPolicyManagerWithGraduationTest
    : public WebAppPolicyManagerTestBase {
 public:
  WebAppPolicyManagerWithGraduationTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kGraduation);
  }

  ~WebAppPolicyManagerWithGraduationTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> managed_profile_;
};

TEST_F(WebAppPolicyManagerWithGraduationTest,
       GraduationNotDisabledWhenAllowed) {
  auto disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.empty());

  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List()
          .Append(static_cast<int>(policy::SystemFeature::kCamera))
          .Append(static_cast<int>(policy::SystemFeature::kOsSettings))
          .Append(static_cast<int>(policy::SystemFeature::kKeyShortcuts)));
  base::Value::Dict graduation_status;
  graduation_status.Set("is_enabled", true);
  profile()->GetPrefs()->SetDict(ash::prefs::kGraduationEnablementStatus,
                                 graduation_status.Clone());

  disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_FALSE(disabled_apps.contains(ash::SystemWebAppType::GRADUATION));
}

TEST_F(WebAppPolicyManagerWithGraduationTest, GraduationDisabledWhenBlocked) {
  auto disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.empty());

  // Add supported system web apps to system features disable list policy.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetUserPref(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::Value::List()
          .Append(static_cast<int>(policy::SystemFeature::kCamera))
          .Append(static_cast<int>(policy::SystemFeature::kOsSettings))
          .Append(static_cast<int>(policy::SystemFeature::kKeyShortcuts)));
  base::Value::Dict graduation_status;
  graduation_status.Set("is_enabled", false);
  profile()->GetPrefs()->SetDict(ash::prefs::kGraduationEnablementStatus,
                                 graduation_status.Clone());

  disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.contains(ash::SystemWebAppType::GRADUATION));
}

class WebAppPolicyManagerWithBocaTest : public WebAppPolicyManagerTestBase {
 public:
  WebAppPolicyManagerWithBocaTest() = default;
  ~WebAppPolicyManagerWithBocaTest() override = default;
};

TEST_F(WebAppPolicyManagerWithBocaTest,
       BocaNotDisabledWhenNotDisabledFromPolicy) {
  auto disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.empty());

  profile()->GetPrefs()->SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "teacher");

  disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_FALSE(disabled_apps.contains(ash::SystemWebAppType::BOCA));
}

TEST_F(WebAppPolicyManagerWithBocaTest, BocaDisabledWhenDisabledFromPolicy) {
  auto disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.empty());
  profile()->GetPrefs()->SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "disabled");
  disabled_apps = policy_manager().GetDisabledSystemWebApps();
  EXPECT_TRUE(disabled_apps.contains(ash::SystemWebAppType::BOCA));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class WebAppPolicyManagerPreventCloseTest
    : public WebAppPolicyManagerTestBase,
      public testing::WithParamInterface<
          std::tuple<bool /*prevent_close_enabled*/,
                     bool /*run_on_os_login_enabled*/>> {
 public:
  WebAppPolicyManagerPreventCloseTest() = default;
  WebAppPolicyManagerPreventCloseTest(
      const WebAppPolicyManagerPreventCloseTest&) = delete;
  WebAppPolicyManagerPreventCloseTest& operator=(
      const WebAppPolicyManagerPreventCloseTest&) = delete;
  ~WebAppPolicyManagerPreventCloseTest() override = default;

  void SetUp() override {
    BuildAndInitFeatureList();
    WebAppPolicyManagerTestBase::SetUp();
  }

  bool prevent_close_enabled() const { return std::get<0>(GetParam()); }

  bool run_on_os_login_enabled() const { return std::get<1>(GetParam()); }

 private:
  void BuildAndInitFeatureList() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (prevent_close_enabled()) {
      enabled_features.push_back(features::kDesktopPWAsPreventClose);
    } else {
      disabled_features.push_back(features::kDesktopPWAsPreventClose);
    }

    if (run_on_os_login_enabled()) {
      enabled_features.push_back(features::kDesktopPWAsRunOnOsLogin);
    } else {
      disabled_features.push_back(features::kDesktopPWAsRunOnOsLogin);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppPolicyManagerPreventCloseTest, WebAppSettingsPreventClose) {
  const char kWebAppSettingConfiguration[] = R"([
    {
      "manifest_id": "*",
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
      "manifest_id": "https://allowed.example/",
      "run_on_os_login": "allowed",
      "prevent_close_after_run_on_os_login": true
    },
    {
      "manifest_id": "https://windowed-only-manually.example/",
      "run_on_os_login": "run_windowed",
      "prevent_close_after_run_on_os_login": true
    },
    {
      "manifest_id": "https://windowed-also-manually.example/",
      "run_on_os_login": "run_windowed",
      "prevent_close_after_run_on_os_login": true
    },
    {
      "manifest_id": "https://windowed.example/",
      "run_on_os_login": "run_windowed",
      "prevent_close_after_run_on_os_login": true
    }
  ])";
  const char kWebAppForceInstallList[] = R"([
    {
      "url": "https://wildcard.example/",
      "default_launch_container": "window"
    },
    {
      "url": "https://tabbed.example/",
      "default_launch_container": "window"
    },
    {
      "url": "https://no-container.example/",
      "default_launch_container": "window"
    },
    {
      "url": "https://allowed.example/",
      "default_launch_container": "window"
    },
    {
      "url": "https://windowed-also-manually.example/",
      "default_launch_container": "window"
    },
    {
      "url": "https://windowed.example/",
      "default_launch_container": "window"
    }
  ])";
  const char kWildcardUrl[] = "https://wildcard.example/";
  const char kAllowedUrl[] = "https://allowed.example/";
  const char kWindowedOnlyManuallyInstalled[] =
      "https://windowed-only-manually.example/";
  const char kWindowedAlsoManuallyInstalled[] =
      "https://windowed-also-manually.example/";

  {
    base::RunLoop loop;
    policy_manager().SetRefreshPolicySettingsCompletedCallbackForTesting(
        loop.QuitClosure());
    SetWebAppSettingsListPref(profile(), kWebAppSettingConfiguration);
    loop.Run();
  }

  {
    base::RunLoop loop;
    policy_manager().SetOnAppsSynchronizedCompletedCallbackForTesting(
        loop.QuitClosure());
    SetWebAppInstallForceListPref(profile(), kWebAppForceInstallList);
    loop.Run();
  }

  // We need to verify that prevent close feature works for app that has
  // multiple install sources and one of them has to be policy. This specific
  // app is already installed by policy and we have to add another install
  // source for that app.
  {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();

    std::optional<webapps::AppId> app_id =
        app_registrar().LookUpAppIdByInstallUrl(
            GURL(kWindowedAlsoManuallyInstalled));
    ASSERT_TRUE(app_id.has_value());

    WebApp* windowed_install_app = update->UpdateApp(app_id.value());
    ASSERT_TRUE(windowed_install_app);
    windowed_install_app->AddSource(WebAppManagement::kSync);
  }

  InstallPwa(kWindowedOnlyManuallyInstalled);

  EXPECT_FALSE(IsPreventCloseEnabled(kWildcardUrl));
  EXPECT_FALSE(IsPreventCloseEnabled(kTabbedUrl));
  EXPECT_FALSE(IsPreventCloseEnabled(kNoContainerUrl));
  EXPECT_FALSE(IsPreventCloseEnabled(kAllowedUrl));
  EXPECT_FALSE(IsPreventCloseEnabled(kWindowedOnlyManuallyInstalled));

  bool expected_windowed_url_status = false;
#if BUILDFLAG(IS_CHROMEOS)
  if (prevent_close_enabled() && run_on_os_login_enabled()) {
    expected_windowed_url_status = true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  EXPECT_EQ(IsPreventCloseEnabled(kWindowedAlsoManuallyInstalled),
            expected_windowed_url_status);
  EXPECT_EQ(IsPreventCloseEnabled(kWindowedUrl), expected_windowed_url_status);
}

INSTANTIATE_TEST_SUITE_P(
    WebAppPolicyManagerPreventCloseTestWithParams,
    WebAppPolicyManagerPreventCloseTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const ::testing::TestParamInfo<
        std::tuple<bool /*prevent_close_enabled*/,
                   bool /*run_on_os_login_enabled*/>>& info) {
      std::string test_name = "Test_";

      if (std::get<0>(info.param)) {
        test_name.append("PreventCloseEnabled_");
      } else {
        test_name.append("PreventCloseDisabled_");
      }

      if (std::get<1>(info.param)) {
        test_name.append("RunOnOsLoginEnabled");
      } else {
        test_name.append("RunOnOsLoginDisabled");
      }

      return test_name;
    });

class WebAppPolicyForceUnregistrationTest : public WebAppTest {
 public:
  WebAppPolicyForceUnregistrationTest() = default;
  ~WebAppPolicyForceUnregistrationTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        web_app::kDesktopPWAsForceUnregisterOSIntegration);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ = OsIntegrationTestOverrideImpl::OverrideForTesting();
    }

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager));

    fake_provider().SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // Blocking required due to file operations in the shortcut override
    // destructor.
    test::UninstallAllWebApps(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

 protected:
  SkBitmap CreateSolidColorIcon(int size, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(size, size);
    bitmap.eraseColor(color);
    return bitmap;
  }

  webapps::AppId InstallWebAppWithShortcuts(
      std::map<SquareSizePx, SkBitmap> icon_map,
      const GURL manifest_id) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>(webapps::ManifestId(manifest_id),
                                            /*start_url=*/manifest_id);
    // The name of the app should also change, otherwise on Mac and Windows, the
    // shortcuts are stored as name(1) and gets wiped out with name.
    info->title = base::UTF8ToUTF16(manifest_id.GetHost());
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->icon_bitmaps.any = std::move(icon_map);
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

  bool IsOsIntegrationAllowed() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    return true;
#else
    return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  }

  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

 private:
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebAppPolicyForceUnregistrationTest,
       RefreshPolicyTrueRemovesOsIntegration) {
  if (!IsOsIntegrationAllowed()) {
    GTEST_SKIP() << "OS integration execution does not work on this OS";
  }

  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map), kWebAppUrl);
  const std::string& app_name =
      provider().registrar_unsafe().GetAppShortName(app_id);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));

  const char kWebAppSettingForceUnregisterApp[] = R"([
    {
      "manifest_id": "https://example.com/path/index.html",
      "force_unregister_os_integration": true
    }
  ])";

  base::test::TestFuture<void> test_future;
  provider()
      .policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          test_future.GetCallback());
  SetWebAppSettingsListPref(profile(), kWebAppSettingForceUnregisterApp);
  EXPECT_TRUE(test_future.Wait());

  ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));
}

TEST_F(WebAppPolicyForceUnregistrationTest,
       RefreshPolicyFalseDoesNotRemovesOsIntegration) {
  if (!IsOsIntegrationAllowed()) {
    GTEST_SKIP() << "OS integration execution does not work on this OS";
  }
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map), kWebAppUrl);
  const std::string& app_name =
      provider().registrar_unsafe().GetAppShortName(app_id);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));

  const char kWebAppSettingForceUnregisterApp[] = R"([
    {
      "manifest_id": "https://example.com/path/index.html",
      "force_unregister_os_integration": false
    }
  ])";

  base::test::TestFuture<void> test_future;
  provider()
      .policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          test_future.GetCallback());
  SetWebAppSettingsListPref(profile(), kWebAppSettingForceUnregisterApp);
  EXPECT_TRUE(test_future.Wait());

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));
}

TEST_F(WebAppPolicyForceUnregistrationTest,
       OtherPoliciesDoNotAffectOsIntegration) {
  if (!IsOsIntegrationAllowed()) {
    GTEST_SKIP() << "OS integration execution does not work on this OS";
  }
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map), kWebAppUrl);
  const std::string& app_name =
      provider().registrar_unsafe().GetAppShortName(app_id);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));

  const char kWebAppSettingForceUnregisterApp[] = R"([
    {
      "manifest_id": "https://example.com/path/index.html",
      "run_on_os_login": "allowed"
    }
  ])";

  base::test::TestFuture<void> test_future;
  provider()
      .policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          test_future.GetCallback());
  SetWebAppSettingsListPref(profile(), kWebAppSettingForceUnregisterApp);
  EXPECT_TRUE(test_future.Wait());

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));
}

TEST_F(WebAppPolicyForceUnregistrationTest,
       ManifestWildcardDoNotAffectOsIntegration) {
  if (!IsOsIntegrationAllowed()) {
    GTEST_SKIP() << "OS integration execution does not work on this OS";
  }
  std::map<SquareSizePx, SkBitmap> icon_map;
  icon_map[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id =
      InstallWebAppWithShortcuts(std::move(icon_map), kWebAppUrl);
  const std::string& app_name =
      provider().registrar_unsafe().GetAppShortName(app_id);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));

  const char kWebAppSettingForceUnregisterApp[] = R"([
    {
      "manifest_id": "*",
      "force_unregister_os_integration": true
    }
  ])";

  base::test::TestFuture<void> test_future;
  provider()
      .policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          test_future.GetCallback());
  SetWebAppSettingsListPref(profile(), kWebAppSettingForceUnregisterApp);
  EXPECT_TRUE(test_future.Wait());

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id, app_name));
}

// Do not call GTEST_SKIP() explicitly here since we do not perform any OS
// integration in this test, and we just verify that the policy itself does not
// work gracefully instead of crashing.
TEST_F(WebAppPolicyForceUnregistrationTest,
       ManifestIdDoesNotExistDoesNotCrash) {
  const char kWebAppSettingForceUnregisterApp[] = R"([
    {
      "manifest_id": "https://unknown.app/",
      "force_unregister_os_integration": true
    }
  ])";

  base::test::TestFuture<void> test_future;
  provider()
      .policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          test_future.GetCallback());
  SetWebAppSettingsListPref(profile(), kWebAppSettingForceUnregisterApp);
  EXPECT_TRUE(test_future.Wait());
}

TEST_F(WebAppPolicyForceUnregistrationTest,
       MultiAppOsIntegrationRemovalBothCorrect) {
  if (!IsOsIntegrationAllowed()) {
    GTEST_SKIP() << "OS integration execution does not work on this OS";
  }
  std::map<SquareSizePx, SkBitmap> icon_map1;
  icon_map1[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map1[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id1 =
      InstallWebAppWithShortcuts(std::move(icon_map1), kWebAppUrl);
  const std::string& app_name1 =
      provider().registrar_unsafe().GetAppShortName(app_id1);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id1, app_name1));

  const GURL manifest_id2 = GURL("https://example_2.com/index.html");
  std::map<SquareSizePx, SkBitmap> icon_map2;
  icon_map2[icon_size::k24] =
      CreateSolidColorIcon(icon_size::k24, SK_ColorGREEN);
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorCYAN);
  const webapps::AppId& app_id2 =
      InstallWebAppWithShortcuts(std::move(icon_map2), manifest_id2);
  const std::string& app_name2 =
      provider().registrar_unsafe().GetAppShortName(app_id2);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id2, app_name2));

  const char kWebAppSettingForceUnregisterApp[] = R"([
    {
      "manifest_id": "https://example.com/path/index.html",
      "force_unregister_os_integration": true
    },
    {
      "manifest_id": "https://example_2.com/index.html",
      "force_unregister_os_integration": true
    }
  ])";

  base::test::TestFuture<void> test_future;
  provider()
      .policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          test_future.GetCallback());
  SetWebAppSettingsListPref(profile(), kWebAppSettingForceUnregisterApp);
  EXPECT_TRUE(test_future.Wait());

  ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id1, app_name1));
  ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id2, app_name2));
}

TEST_F(WebAppPolicyForceUnregistrationTest,
       MultiAppOsIntegrationRemovalOneCorrect) {
  if (!IsOsIntegrationAllowed()) {
    GTEST_SKIP() << "OS integration execution does not work on this OS";
  }
  std::map<SquareSizePx, SkBitmap> icon_map1;
  icon_map1[icon_size::k24] = CreateSolidColorIcon(icon_size::k24, SK_ColorRED);
  icon_map1[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorGREEN);
  const webapps::AppId& app_id1 =
      InstallWebAppWithShortcuts(std::move(icon_map1), kWebAppUrl);
  const std::string& app_name1 =
      provider().registrar_unsafe().GetAppShortName(app_id1);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id1, app_name1));

  const GURL manifest_id2 = GURL("https://example_2.com/index.html");
  std::map<SquareSizePx, SkBitmap> icon_map2;
  icon_map2[icon_size::k24] =
      CreateSolidColorIcon(icon_size::k24, SK_ColorGREEN);
  icon_map2[icon_size::k128] =
      CreateSolidColorIcon(icon_size::k128, SK_ColorCYAN);
  const webapps::AppId& app_id2 =
      InstallWebAppWithShortcuts(std::move(icon_map2), manifest_id2);
  const std::string& app_name2 =
      provider().registrar_unsafe().GetAppShortName(app_id2);

  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id2, app_name2));

  // Have a typo in one for manifest_id of 2nd app, or have it be non-existent.
  const char kWebAppSettingForceUnregisterApp[] = R"([
    {
      "manifest_id": "https://example.com/path/index.html",
      "force_unregister_os_integration": true
    },
    {
      "manifest_id": "https://example_2.com/invalid_index.html",
      "force_unregister_os_integration": true
    }
  ])";

  base::test::TestFuture<void> test_future;
  provider()
      .policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          test_future.GetCallback());
  SetWebAppSettingsListPref(profile(), kWebAppSettingForceUnregisterApp);
  EXPECT_TRUE(test_future.Wait());

  ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id1, app_name1));
  ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsShortcutCreated(
      profile(), app_id2, app_name2));
}
}  // namespace web_app
