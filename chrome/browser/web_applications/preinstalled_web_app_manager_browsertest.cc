// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"

#include <string_view>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/ssl/ssl_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/test/app_list_test_api.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace web_app {

namespace {

constexpr char kBaseDataDir[] = "chrome/test/data/banners";

// start_url in manifest.json matches navigation url for the simple
// manifest_test_page.html.
constexpr char kSimpleManifestStartUrl[] =
    "https://example.org/manifest_test_page.html";

constexpr char kNoManifestTestPageStartUrl[] =
    "https://example.org/no_manifest_test_page.html";

// Performs blocking IO operations.
base::FilePath GetDataFilePath(const base::FilePath& relative_path,
                               bool* path_exists) {
  base::ScopedAllowBlockingForTesting allow_io;

  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path));
  base::FilePath path = root_path.Append(relative_path);
  *path_exists = base::PathExists(path);
  return path;
}

#if BUILDFLAG(IS_CHROMEOS)
void ExpectInitialManifestFieldsFromBasicWebApp(WebAppIconManager& icon_manager,
                                                const WebApp* web_app,
                                                const GURL& expect_start_url,
                                                const GURL& expect_scope) {
  // Manifest fields:
  EXPECT_EQ(web_app->untranslated_name(), "Basic web app");
  EXPECT_EQ(web_app->start_url().spec(), expect_start_url);
  EXPECT_EQ(web_app->scope().spec(), expect_scope);
  EXPECT_EQ(web_app->display_mode(), DisplayMode::kStandalone);
  EXPECT_FALSE(web_app->theme_color().has_value());

  EXPECT_FALSE(web_app->sync_proto().has_theme_color());
  EXPECT_EQ("Basic web app", web_app->sync_proto().name());
  EXPECT_EQ(expect_scope.spec(), web_app->sync_proto().scope());

  ASSERT_EQ(2, web_app->sync_proto().icon_infos_size());

  EXPECT_EQ(expect_start_url.Resolve("basic-48.png").spec(),
            web_app->sync_proto().icon_infos(0).url());
  EXPECT_EQ(48, web_app->sync_proto().icon_infos(0).size_in_px());
  EXPECT_EQ(sync_pb::WebAppIconInfo_Purpose_ANY,
            web_app->sync_proto().icon_infos(0).purpose());

  EXPECT_EQ(expect_start_url.Resolve("basic-192.png").spec(),
            web_app->sync_proto().icon_infos(1).url());
  EXPECT_EQ(192, web_app->sync_proto().icon_infos(1).size_in_px());
  EXPECT_EQ(sync_pb::WebAppIconInfo_Purpose_ANY,
            web_app->sync_proto().icon_infos(1).purpose());

  // Manifest Resources: This is chrome/test/data/web_apps/basic-192.png
  EXPECT_EQ(IconManagerReadAppIconPixel(icon_manager, web_app->app_id(),
                                        /*size_px=*/192),
            SK_ColorBLACK);

  // User preferences:
  EXPECT_EQ(web_app->user_display_mode(), mojom::UserDisplayMode::kStandalone);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class PreinstalledWebAppManagerBrowserTestBase
    : virtual public InProcessBrowserTest {
 public:
  PreinstalledWebAppManagerBrowserTestBase()
      : skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()) {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        WebAppProvider::GetForTest(browser()->profile()));
  }

  void TearDownOnMainThread() override {
    ResetInterceptor();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void InitUrlLoaderInterceptor() {
    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // a stable app_id across tests requires stable origin, whereas
    // EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) -> bool {
              std::string relative_request = base::StrCat(
                  {kBaseDataDir, params->url_request.url.path_piece()});
              base::FilePath relative_path =
                  base::FilePath().AppendASCII(relative_request);

              bool path_exists = false;
              base::FilePath path =
                  GetDataFilePath(relative_path, &path_exists);
              if (!path_exists)
                return /*intercepted=*/false;

              // Provide fake SSLInfo to avoid NOT_FROM_SECURE_ORIGIN error in
              // InstallableManager::GetData().
              net::SSLInfo ssl_info;
              CreateFakeSslInfoCertificate(&ssl_info);

              content::URLLoaderInterceptor::WriteResponse(
                  path, params->client.get(), /*headers=*/nullptr, ssl_info,
                  params->url_request.url);

              return /*intercepted=*/true;
            }));
  }

  GURL GetAppUrl() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }

  WebAppIconManager& icon_manager() { return provider().icon_manager(); }

  PreinstalledWebAppManager& manager() {
    return provider().preinstalled_web_app_manager();
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  void SyncEmptyConfigs() {
    base::Value::List app_configs;
    base::AutoReset<const base::Value::List*> configs_for_testing =
        PreinstalledWebAppManager::SetConfigsForTesting(&app_configs);

    base::RunLoop run_loop;
    WebAppProvider::GetForTest(profile())
        ->preinstalled_web_app_manager()
        .LoadAndSynchronizeForTesting(base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, webapps::UninstallResultCode>
                    uninstall_results) {
              EXPECT_EQ(install_results.size(), 0u);
              EXPECT_EQ(uninstall_results.size(), 0u);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Mocks "icon.png" as chrome/test/data/web_apps/blue-192.png.
  std::optional<webapps::InstallResultCode> SyncPreinstalledAppConfig(
      const GURL& install_url,
      std::string_view app_config_string) {
    base::FilePath test_config_dir(FILE_PATH_LITERAL("test_dir"));
    SetPreinstalledWebAppConfigDirForTesting(&test_config_dir);

    base::FilePath source_root_dir;
    CHECK(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir));
    base::FilePath test_icon_path =
        source_root_dir.Append(GetChromeTestDataDir())
            .AppendASCII("web_apps/blue-192.png");
    scoped_refptr<TestFileUtils> file_utils = TestFileUtils::Create(
        {{base::FilePath(FILE_PATH_LITERAL("test_dir/icon.png")),
          test_icon_path}});
    base::AutoReset<FileUtilsWrapper*> file_utils_for_testing =
        PreinstalledWebAppManager::SetFileUtilsForTesting(file_utils.get());

    base::Value::List app_configs;
    auto json_parse_result =
        base::JSONReader::ReadAndReturnValueWithError(app_config_string);
    EXPECT_TRUE(json_parse_result.has_value())
        << "JSON parse error: " << json_parse_result.error().message;
    if (!json_parse_result.has_value())
      return std::nullopt;
    app_configs.Append(std::move(*json_parse_result));
    base::AutoReset<const base::Value::List*> configs_for_testing =
        PreinstalledWebAppManager::SetConfigsForTesting(&app_configs);

    std::optional<webapps::InstallResultCode> code;
    base::RunLoop sync_run_loop;
    WebAppProvider::GetForTest(profile())
        ->preinstalled_web_app_manager()
        .LoadAndSynchronizeForTesting(base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, webapps::UninstallResultCode>
                    uninstall_results) {
              auto it = install_results.find(install_url);
              if (it != install_results.end())
                code = it->second.code;
              sync_run_loop.Quit();
            }));
    sync_run_loop.Run();

    SetPreinstalledWebAppConfigDirForTesting(nullptr);

    return code;
  }

  ~PreinstalledWebAppManagerBrowserTestBase() override = default;

  Profile* profile() { return browser()->profile(); }

 protected:
  void ResetInterceptor() { url_loader_interceptor_.reset(); }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
};

class PreinstalledWebAppManagerBrowserTest
    : public PreinstalledWebAppManagerBrowserTestBase {
 public:
  PreinstalledWebAppManagerBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitWithFeatures(
        {features::kRecordWebAppDebugInfo},
        {chromeos::features::kPreinstalledWebAppsCoreOnly});
#else
    feature_list_.InitWithFeatures({features::kRecordWebAppDebugInfo}, {});
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsBasic) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "test_launch_params"
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {start_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(start_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  GURL launch_url =
      embedded_test_server()->GetURL("/web_apps/basic.html?test_launch_params");
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  // This is required to allow launching to work.
  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsDuplicate) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html");
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html?query_params=in&start=url");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "query_params=in"
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {install_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(install_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  // We should not duplicate the query param if start_url already has it.
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), start_url);

  // This is required to allow launching to work.
  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      start_url);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsMultiple) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  GURL launch_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?more=than&one=query&param");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "more=than&one=query&param"
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {start_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(start_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  // This is required to allow launching to work.
  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsComplex) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html");
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html?query_params=in&start=url");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "!@#$$%^*&)("
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {install_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(install_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  GURL launch_url = embedded_test_server()->GetURL(
      "/web_apps/"
      "query_params_in_start_url.html?query_params=in&start=url&!@%23$%^*&)(");
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

class PreinstalledWebAppManagerExtensionBrowserTest
    : public extensions::ExtensionBrowserTest,
      public PreinstalledWebAppManagerBrowserTest {
 public:
  PreinstalledWebAppManagerExtensionBrowserTest()
      : enable_chrome_apps_(
            &extensions::testing::g_enable_chrome_apps_for_testing,
            true) {}
  ~PreinstalledWebAppManagerExtensionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        WebAppProvider::GetForTest(browser()->profile()));
  }
  void TearDownOnMainThread() override {
    ResetInterceptor();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

 private:
  base::AutoReset<bool> enable_chrome_apps_;
};

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerExtensionBrowserTest,
                       UninstallAndReplace) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install Chrome app to be replaced.
  const char kChromeAppDirectory[] = "app";
  const char kChromeAppName[] = "App Test";
  const extensions::Extension* app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII(kChromeAppDirectory), 1,
      extensions::mojom::ManifestLocation::kInternal,
      extensions::Extension::NO_FLAGS);
  EXPECT_EQ(app->name(), kChromeAppName);

  // Start listening for Chrome app uninstall.
  extensions::TestExtensionRegistryObserver uninstall_observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "uninstall_and_replace": ["$2"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec(), app->id()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  // Chrome app should get uninstalled.
  scoped_refptr<const extensions::Extension> uninstalled_app =
      uninstall_observer.WaitForExtensionUninstalled();
  EXPECT_EQ(app, uninstalled_app.get());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PreinstalledAppsPrefInstall) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());
  profile()->GetPrefs()->SetString(prefs::kPreinstalledApps, "install");

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PreinstalledAppsPrefNoinstall) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());
  profile()->GetPrefs()->SetString(prefs::kPreinstalledApps, "noinstall");

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config), std::nullopt);
}

const char kOnlyIfPreviouslyPreinstalled_PreviousConfig[] = R"({
  "app_url": "$1",
  "launch_container": "window",
  "user_type": ["unmanaged"]
})";
const char kOnlyIfPreviouslyPreinstalled_NextConfig[] = R"({
  "app_url": "$1",
  "launch_container": "window",
  "user_type": ["unmanaged"],
  "only_if_previously_preinstalled": true
})";

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PRE_OnlyIfPreviouslyPreinstalled_AppPreserved) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string prev_app_config = base::ReplaceStringPlaceholders(
      kOnlyIfPreviouslyPreinstalled_PreviousConfig, {kSimpleManifestStartUrl},
      nullptr);

  // The user had the app installed.
  EXPECT_EQ(
      SyncPreinstalledAppConfig(GURL{kSimpleManifestStartUrl}, prev_app_config),
      webapps::InstallResultCode::kSuccessNewInstall);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kSimpleManifestStartUrl});
  EXPECT_TRUE(registrar().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OnlyIfPreviouslyPreinstalled_AppPreserved) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string next_app_config =
      base::ReplaceStringPlaceholders(kOnlyIfPreviouslyPreinstalled_NextConfig,
                                      {kSimpleManifestStartUrl}, nullptr);

  // The user still has the app.
  EXPECT_EQ(
      SyncPreinstalledAppConfig(GURL{kSimpleManifestStartUrl}, next_app_config),
      webapps::InstallResultCode::kSuccessAlreadyInstalled);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kSimpleManifestStartUrl});
  EXPECT_TRUE(registrar().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PRE_OnlyIfPreviouslyPreinstalled_NoAppPreinstalled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string prev_app_config = base::ReplaceStringPlaceholders(
      kOnlyIfPreviouslyPreinstalled_PreviousConfig,
      {kNoManifestTestPageStartUrl}, nullptr);

  EXPECT_EQ(SyncPreinstalledAppConfig(GURL{kNoManifestTestPageStartUrl},
                                      prev_app_config),
            webapps::InstallResultCode::kNotValidManifestForWebApp);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kNoManifestTestPageStartUrl});
  EXPECT_FALSE(registrar().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OnlyIfPreviouslyPreinstalled_NoAppPreinstalled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string next_app_config =
      base::ReplaceStringPlaceholders(kOnlyIfPreviouslyPreinstalled_NextConfig,
                                      {kNoManifestTestPageStartUrl}, nullptr);

  // The user has no the app.
  EXPECT_EQ(SyncPreinstalledAppConfig(GURL{kNoManifestTestPageStartUrl},
                                      next_app_config),
            std::nullopt);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kNoManifestTestPageStartUrl});
  EXPECT_FALSE(registrar().IsInstalled(app_id));
}

const char kFeatureNameOrInstalledConfig[] = R"({
  "app_url": "$1",
  "launch_container": "window",
  "user_type": ["unmanaged"],
  "feature_name_or_installed": "test_feature"
})";

// When the "feature_name_or_installed" feature is enabled, the app should be
// installed.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       GateOnFeatureNameOrInstalled_InstallWhenEnabled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  base::AutoReset<bool> enable_feature =
      SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();

  std::string app_config = base::ReplaceStringPlaceholders(
      kFeatureNameOrInstalledConfig, {GetAppUrl().spec()}, nullptr);

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  EXPECT_TRUE(registrar().IsInstalled(app_id));
}

// When the "feature_name_or_installed" feature is disabled, the app should not
// be installed.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       GateOnFeatureNameOrInstalled_IgnoreWhenDisabled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string app_config = base::ReplaceStringPlaceholders(
      kFeatureNameOrInstalledConfig, {GetAppUrl().spec()}, nullptr);

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config), std::nullopt);

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  EXPECT_FALSE(registrar().IsInstalled(app_id));
}

// When the "feature_name_or_installed" feature is disabled, any existing
// preinstalled app should not be uninstalled.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       Installed_DoNotUninstallWhenDisabled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string app_config = base::ReplaceStringPlaceholders(
      kFeatureNameOrInstalledConfig, {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  {
    base::AutoReset<bool> enable_feature =
        SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
              webapps::InstallResultCode::kSuccessNewInstall);

    EXPECT_TRUE(registrar().IsInstalled(app_id));
  }

  {
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
              webapps::InstallResultCode::kSuccessAlreadyInstalled);

    EXPECT_TRUE(registrar().IsInstalled(app_id));
  }
}

// Preinstalled apps which are user uninstalled become ignored configs.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       DisableForPreinstalledAppsInConfig) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string config = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  // Preinstall web app.
  {
    base::HistogramTester tester;
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), config),
              webapps::InstallResultCode::kSuccessNewInstall);
    EXPECT_TRUE(registrar().IsInstalled(app_id));
    tester.ExpectUniqueSample("WebApp.Preinstalled.DisabledReason",
                              /*kNotDisabled*/ 0, 1);
  }

  // Mark web app as user uninstalled without uninstalling it.
  // This is an erroneous state that users have gotten into in the past via
  // database migration bugs, see crbug.com/1393284 and crbug.com/1363004 for
  // past incidents.
  {
    UserUninstalledPreinstalledWebAppPrefs prefs(profile()->GetPrefs());
    prefs.Add(app_id, {GetAppUrl()});
    ASSERT_EQ(app_id, prefs.LookUpAppIdByInstallUrl(GetAppUrl()));
  }

  // Check web app does not get uninstalled by PWAM sync.
  {
    base::HistogramTester tester;
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), config),
              webapps::InstallResultCode::kSuccessAlreadyInstalled);
    EXPECT_TRUE(registrar().IsInstalled(app_id));
    tester.ExpectUniqueSample("WebApp.Preinstalled.DisabledReason",
                              /*kIgnorePreviouslyUninstalledByUser*/ 17, 1);
  }

  // Actually uninstall web app.
  {
    base::test::TestFuture<webapps::UninstallResultCode> future;
    provider().scheduler().RemoveUserUninstallableManagements(
        app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
    ASSERT_EQ(future.Get(), webapps::UninstallResultCode::kAppRemoved);
    ASSERT_FALSE(registrar().IsInstalled(app_id));
  }

  // Check web app does not get installed by PWAM sync.
  {
    base::HistogramTester tester;
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), config), std::nullopt);
    EXPECT_FALSE(registrar().IsInstalled(app_id));
    tester.ExpectUniqueSample("WebApp.Preinstalled.DisabledReason",
                              /*kIgnorePreviouslyUninstalledByUser*/ 17, 1);
  }
}

// Preinstalled apps which are user uninstalled are included
// in the config passed to the ExternallyManagedAppInstallManager if
// |override_previous_user_uninstall| is true.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PreinstalledAppsUninstallOverride) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  base::AutoReset<bool> override_previous_user_uninstall_config =
      PreinstalledWebAppManager::
          OverridePreviousUserUninstallConfigForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  ASSERT_FALSE(registrar().IsInstalled(app_id));
  UserUninstalledPreinstalledWebAppPrefs prefs(profile()->GetPrefs());
  prefs.Add(app_id, {GetAppUrl()});

  // Verify prefs have the proper data.
  EXPECT_EQ(1, prefs.Size());
  EXPECT_EQ(app_id, prefs.LookUpAppIdByInstallUrl(GetAppUrl()));

  // On sync across configs, app is installed because
  // |override_previous_user_uninstall| is true.
  const auto& ignore_configs = manager().debug_info()->ignore_configs;
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(ignore_configs.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       IgnoreCorruptUserUninstalledPreinstalledWebAppPrefs) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  EXPECT_TRUE(registrar().IsInstalledByDefaultManagement(app_id));

  // Simulate the effects of https://crbug.com/1359205 by adding an installed
  // preinstalled web app to the "has been uninstalled by the user" pref even
  // though the web app is still kDefault installed.
  UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
      .Add(app_id, {GetAppUrl()});

  // Check that the PreinstalledWebAppManager doesn't uninstall the web app
  // just because the prefs say it's uninstalled.
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessAlreadyInstalled);
  EXPECT_TRUE(registrar().IsInstalledByDefaultManagement(app_id));
}

// The offline manifest JSON config functionality is only available on Chrome
// OS.
#if BUILDFLAG(IS_CHROMEOS)

// Check that offline fallback installs work offline.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OfflineFallbackManifestSiteOffline) {
  constexpr char kAppInstallUrl[] = "https://offline-site.com/install.html";
  constexpr char kAppName[] = "Offline app name";
  constexpr char kAppStartUrl[] = "https://offline-site.com/start.html";
  constexpr char kAppScope[] = "https://offline-site.com/";

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kAppStartUrl));
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "offline_manifest": {
          "name": "$2",
          "start_url": "$3",
          "scope": "$4",
          "display": "minimal-ui",
          "theme_color_argb_hex": "AABBCCDD",
          "icon_any_pngs": ["icon.png"]
        }
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {kAppInstallUrl, kAppName, kAppStartUrl, kAppScope},
      nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GURL(kAppInstallUrl), app_config),
            webapps::InstallResultCode::kSuccessOfflineFallbackInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), kAppName);
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), kAppStartUrl);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), kAppScope);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppDisplayMode(app_id), DisplayMode::kMinimalUi);
  // theme_color must be installed opaque.
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
  EXPECT_EQ(
      IconManagerReadAppIconPixel(icon_manager(), app_id, /*size_px=*/192),
      SK_ColorBLUE);
}

// Check that offline fallback installs attempt fetching the install_url.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OfflineFallbackManifestSiteOnline) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This install_url serves a manifest with different values to what we specify
  // in the offline_manifest. Check that it gets used instead of the
  // offline_manifest.
  GURL install_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  GURL offline_start_url = embedded_test_server()->GetURL(
      "/web_apps/offline-only-start-url-that-does-not-exist.html");
  GURL scope = embedded_test_server()->GetURL("/web_apps/");

  webapps::AppId offline_app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, offline_start_url);
  EXPECT_FALSE(registrar().IsInstalled(offline_app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
          "app_url": "$1",
          "launch_container": "window",
          "user_type": ["unmanaged"],
          "offline_manifest": {
            "name": "Offline only app name",
            "start_url": "$2",
            "scope": "$3",
            "display": "minimal-ui",
            "theme_color_argb_hex": "AABBCCDD",
            "icon_any_pngs": ["icon.png"]
          }
        })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate,
      {install_url.spec(), offline_start_url.spec(), scope.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(install_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_FALSE(registrar().IsInstalled(offline_app_id));

  // basic.html's manifest start_url is basic.html.
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, install_url);
  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), "Basic web app");
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), install_url);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), scope);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppDisplayMode(app_id), DisplayMode::kStandalone);
}

// Check that offline only installs work offline.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OfflineOnlyManifestSiteOffline) {
  constexpr char kAppInstallUrl[] = "https://offline-site.com/install.html";
  constexpr char kAppName[] = "Offline app name";
  constexpr char kAppStartUrl[] = "https://offline-site.com/start.html";
  constexpr char kAppScope[] = "https://offline-site.com/";

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kAppStartUrl));
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "only_use_offline_manifest": true,
        "offline_manifest": {
          "name": "$2",
          "start_url": "$3",
          "scope": "$4",
          "display": "minimal-ui",
          "theme_color_argb_hex": "AABBCCDD",
          "icon_any_pngs": ["icon.png"]
        }
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {kAppInstallUrl, kAppName, kAppStartUrl, kAppScope},
      nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GURL(kAppInstallUrl), app_config),
            webapps::InstallResultCode::kSuccessOfflineOnlyInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), kAppName);
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), kAppStartUrl);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), kAppScope);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppDisplayMode(app_id), DisplayMode::kMinimalUi);
  // theme_color must be installed opaque.
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
  EXPECT_EQ(
      IconManagerReadAppIconPixel(icon_manager(), app_id, /*size_px=*/192),
      SK_ColorBLUE);
}

// Check that offline only installs don't fetch from the install_url.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OfflineOnlyManifestSiteOnline) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This install_url serves a manifest with different values to what we specify
  // in the offline_manifest. Check that it doesn't get used.
  GURL install_url = GetAppUrl();
  const char kAppName[] = "Offline only app name";
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/offline-only-start-url-that-does-not-exist.html");
  GURL scope = embedded_test_server()->GetURL("/web_apps/");

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "only_use_offline_manifest": true,
        "offline_manifest": {
          "name": "$2",
          "start_url": "$3",
          "scope": "$4",
          "display": "minimal-ui",
          "theme_color_argb_hex": "AABBCCDD",
          "icon_any_pngs": ["icon.png"]
        }
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate,
      {install_url.spec(), kAppName, start_url.spec(), scope.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(install_url, app_config),
            webapps::InstallResultCode::kSuccessOfflineOnlyInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), kAppName);
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), scope);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppDisplayMode(app_id), DisplayMode::kMinimalUi);
  // theme_color must be installed opaque.
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
  EXPECT_EQ(
      IconManagerReadAppIconPixel(icon_manager(), app_id, /*size_px=*/192),
      SK_ColorBLUE);
}

const char kOnlyForNewUsersInstallUrl[] = "https://example.org/";
const char kOnlyForNewUsersConfig[] = R"({
    "app_url": "https://example.org/",
    "launch_container": "window",
    "user_type": ["unmanaged"],
    "only_for_new_users": true,
    "only_use_offline_manifest": true,
    "offline_manifest": {
      "name": "Test",
      "start_url": "https://example.org/",
      "scope": "https://example.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  })";

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PRE_OnlyForNewUsersWithNewUser) {
  // Install a policy app first to check that it doesn't interfere.
  {
    base::RunLoop run_loop;
    WebAppPolicyManager& policy_manager =
        WebAppProvider::GetForTest(profile())->policy_manager();
    policy_manager.SetOnAppsSynchronizedCompletedCallbackForTesting(
        run_loop.QuitClosure());
    const char kWebAppPolicy[] = R"([{
      "url": "https://policy-example.org/",
      "default_launch_container": "window"
    }])";
    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                               base::JSONReader::Read(kWebAppPolicy).value());
    run_loop.Run();
  }

  // New user should have the app installed.
  EXPECT_EQ(SyncPreinstalledAppConfig(GURL(kOnlyForNewUsersInstallUrl),
                                      kOnlyForNewUsersConfig),
            webapps::InstallResultCode::kSuccessOfflineOnlyInstall);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OnlyForNewUsersWithNewUser) {
  // App should persist after user stops being a new user.
  EXPECT_EQ(SyncPreinstalledAppConfig(GURL(kOnlyForNewUsersInstallUrl),
                                      kOnlyForNewUsersConfig),
            webapps::InstallResultCode::kSuccessAlreadyInstalled);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PRE_OnlyForNewUsersWithOldUser) {
  // Simulate running Chrome without the configs present.
  SyncEmptyConfigs();
}
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OnlyForNewUsersWithOldUser) {
  // This instance of Chrome should be considered not a new user after the
  // previous PRE_ launch and sync.
  EXPECT_EQ(SyncPreinstalledAppConfig(GURL(kOnlyForNewUsersInstallUrl),
                                      kOnlyForNewUsersConfig),
            std::nullopt);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest, OemInstalled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(),
                                      base::ReplaceStringPlaceholders(
                                          R"({
                "app_url": "$1",
                "launch_container": "window",
                "oem_installed": true,
                "user_type": ["unmanaged"]
              })",
                                          {GetAppUrl().spec()}, nullptr)),
            webapps::InstallResultCode::kSuccessNewInstall);

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  EXPECT_TRUE(registrar().WasInstalledByOem(app_id));

  // Wait for app service to see the newly installed app.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());

  apps::InstallReason install_reason = apps::InstallReason::kUnknown;
  proxy->AppRegistryCache().ForOneApp(app_id,
                                      [&](const apps::AppUpdate& update) {
                                        install_reason = update.InstallReason();
                                      });

  EXPECT_EQ(install_reason, apps::InstallReason::kOem);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
namespace {
ui::TouchscreenDevice CreateTouchDevice(ui::InputDeviceType type,
                                        bool stylus_support) {
  ui::TouchscreenDevice touch_device = ui::TouchscreenDevice();
  touch_device.type = type;
  touch_device.has_stylus = stylus_support;
  return touch_device;
}
}  // namespace
#endif

IN_PROC_BROWSER_TEST_F(
    PreinstalledWebAppManagerBrowserTest,
    DisableIfTouchscreenWithStylusNotSupported_NoStylusSupport) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->has_stylus_enabled_touchscreen = false;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#else
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {CreateTouchDevice(ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                         /*stylus_support=*/false)});
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "disable_if_touchscreen_with_stylus_not_supported": true,
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  const auto& ignore_configs = manager().debug_info()->ignore_configs;
  constexpr char kErrorMessage[] =
      " ignore because the device does not have a built-in touchscreen with "
      "stylus support.";

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest), std::nullopt);
  EXPECT_FALSE(registrar().IsInstalled(app_id));
  EXPECT_EQ(ignore_configs.size(), 1u);
  EXPECT_EQ(ignore_configs.back().second, GetAppUrl().spec() + kErrorMessage);
}

IN_PROC_BROWSER_TEST_F(
    PreinstalledWebAppManagerBrowserTest,
    DisableIfTouchscreenWithStylusNotSupported_HasStylusSupport) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->has_stylus_enabled_touchscreen = true;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#else
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {CreateTouchDevice(ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                         /*stylus_support=*/true)});
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "disable_if_touchscreen_with_stylus_not_supported": true,
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(registrar().IsInstalled(app_id));
}

// Verify that stylus detection works even if DeviceDataManager is slow to
// initialize.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       DisableIfTouchscreenWithStylusStartupDelay) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->has_stylus_enabled_touchscreen = true;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "disable_if_touchscreen_with_stylus_not_supported": true,
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  // Clear out the device list and re-initialize it after a delay. Web app
  // installation should wait for this to be ready.
  ui::DeviceDataManager::GetInstance()->ResetDeviceListsForTest();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([]() {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
        // Create a built-in touchscreen device with stylus support
        // and add it to the device. Lacros does not use DeviceDataManager to
        // determine whether stylus is supported, but still needs device lists
        // to be complete before continuing.
        ui::DeviceDataManagerTestApi().SetTouchscreenDevices({CreateTouchDevice(
            ui::InputDeviceType::INPUT_DEVICE_INTERNAL, true)});
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
        ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
      }),
      base::Milliseconds(500));

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessNewInstall);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Disabled due to test flakiness. https://crbug.com/1267164.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       DISABLED_UninstallFromTwoItemAppListFolder) {
  GURL preinstalled_app_start_url("https://example.org/");
  GURL user_app_start_url("https://test.org/");

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  AppListClientImpl::GetInstance()->UpdateProfile();
  ash::AppListTestApi app_list_test_api;
  app_list::AppListSyncableService* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());

  // Install default app.
  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "only_use_offline_manifest": true,
        "offline_manifest": {
          "name": "Test default app",
          "display": "standalone",
          "start_url": "$1",
          "scope": "$1",
          "icon_any_pngs": ["icon.png"]
        }
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {preinstalled_app_start_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(preinstalled_app_start_url, app_config),
            webapps::InstallResultCode::kSuccessOfflineOnlyInstall);
  webapps::AppId preinstalled_app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, preinstalled_app_start_url);

  // Install user app.
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(user_app_start_url);
  install_info->title = u"Test user app";
  webapps::AppId user_app_id =
      web_app::test::InstallWebApp(profile(), std::move(install_info));

  // Put apps in app list folder.
  std::string folder_id = app_list_test_api.CreateFolderWithApps(
      {preinstalled_app_id, user_app_id});
  EXPECT_EQ(
      app_list_syncable_service->GetSyncItem(preinstalled_app_id)->parent_id,
      folder_id);
  EXPECT_EQ(app_list_syncable_service->GetSyncItem(user_app_id)->parent_id,
            folder_id);

  // Uninstall default app.
  proxy->UninstallSilently(preinstalled_app_id,
                           apps::UninstallSource::kUnknown);

  // Default app should be removed from local app list but remain in sync list.
  EXPECT_FALSE(registrar().IsInstalled(preinstalled_app_id));
  EXPECT_TRUE(registrar().IsInstalled(user_app_id));
  EXPECT_FALSE(app_list_test_api.HasApp(preinstalled_app_id));
  EXPECT_TRUE(app_list_test_api.HasApp(user_app_id));
  EXPECT_EQ(
      app_list_syncable_service->GetSyncItem(preinstalled_app_id)->parent_id,
      "");
  EXPECT_EQ(app_list_syncable_service->GetSyncItem(user_app_id)->parent_id, "");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Check that offline only installs don't overwrite fresh online manifest
// obtained via sync install.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OfflineOnlyManifest_SiteAlreadyInstalledFromSync) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url = GetAppUrl();
  GURL start_url = install_url;
  GURL scope = embedded_test_server()->GetURL("/web_apps/");

  const webapps::AppId app_id = InstallWebAppFromPage(browser(), install_url);

  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_TRUE(web_app->IsSynced());
  EXPECT_FALSE(web_app->IsPreinstalledApp());

  {
    SCOPED_TRACE("Expect initial manifest fields from basic.html web app.");
    ExpectInitialManifestFieldsFromBasicWebApp(icon_manager(), web_app,
                                               start_url, scope);
  }

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "tab",
        "user_type": ["unmanaged"],
        "only_use_offline_manifest": true,
        "offline_manifest": {
          "name": "$2",
          "start_url": "$3",
          "scope": "$4",
          "display": "minimal-ui",
          "theme_color_argb_hex": "AABBCCDD",
          "icon_any_pngs": ["icon.png"]
        }
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate,
      {install_url.spec(), "Overwrite app name", start_url.spec(),
       "https://overwrite.scope/"},
      nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(install_url, app_config),
            webapps::InstallResultCode::kSuccessOfflineOnlyInstall);

  EXPECT_EQ(web_app, registrar().GetAppById(app_id));

  EXPECT_TRUE(web_app->IsSynced());
  EXPECT_TRUE(web_app->IsPreinstalledApp());

  {
    SCOPED_TRACE(
        "Expect same manifest fields from basic.html web app, no overwrites.");
    ExpectInitialManifestFieldsFromBasicWebApp(icon_manager(), web_app,
                                               start_url, scope);
  }
}

class PreinstalledWebAppManagerWithCloudGamingBrowserTest
    : public PreinstalledWebAppManagerBrowserTest {
 public:
  PreinstalledWebAppManagerWithCloudGamingBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->is_cloud_gaming_device = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
#else
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCloudGamingDevice);
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the custom behavior for the "CloudGamingDevice" feature works on
// both Ash and Lacros.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerWithCloudGamingBrowserTest,
                       GateOnCloudGamingFeature) {
  ASSERT_TRUE(embedded_test_server()->Start());

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "feature_name": "$2"
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate,
      {GetAppUrl().spec(), chromeos::features::kCloudGamingDevice.name},
      nullptr);

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class PreinstalledWebAppManagerPreferredAppForSupportedLinksBrowserTest
    : public PreinstalledWebAppManagerBrowserTest,
      public ::testing::WithParamInterface<
          /*is_preferred_app_for_supported_links=*/bool> {
 public:
  bool IsPreferredAppForSupportedLinks() const { return GetParam(); }

  void RemoveSupportedLinksPreference(const webapps::AppId& app_id) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // NOTE: CrosAPI doesn't implement `RemoveSupportedLinksPreference()`.
    loopback_crosapi_->RemoveSupportedLinksPreference(app_id);
    WaitForSupportedLinksPreference(app_id, /*is_preferred_app=*/false);
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
    apps_util::RemoveSupportedLinksPreferenceAndWait(profile(), app_id);
#endif
  }

  void WaitForSupportedLinksPreference(const webapps::AppId& app_id,
                                       bool is_preferred_app) {
    apps_util::PreferredAppUpdateWaiter(
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->PreferredAppsList(),
        app_id, is_preferred_app)
        .Wait();
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
 private:
  // PreinstalledWebAppManagerBrowserTest:
  void SetUpOnMainThread() override {
    PreinstalledWebAppManagerBrowserTest::SetUpOnMainThread();
    loopback_crosapi_ =
        std::make_unique<LoopbackCrosapiAppServiceProxy>(profile());
  }

  void TearDownOnMainThread() override {
    PreinstalledWebAppManagerBrowserTest::TearDownOnMainThread();
    loopback_crosapi_.reset();
  }

  std::unique_ptr<LoopbackCrosapiAppServiceProxy> loopback_crosapi_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PreinstalledWebAppManagerPreferredAppForSupportedLinksBrowserTest,
    /*is_preferred_app_for_supported_links=*/::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    PreinstalledWebAppManagerPreferredAppForSupportedLinksBrowserTest,
    MaybeSetPreferredAppForSupportedLinks) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "is_preferred_app_for_supported_links": $2,
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec(),
       IsPreferredAppForSupportedLinks() ? "true" : "false"},
      nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  // Install the app for the first time.
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Verify that the app is the preferred app if requested in the manifest.
  WaitForSupportedLinksPreference(app_id, IsPreferredAppForSupportedLinks());

  // Clear the preferred app.
  RemoveSupportedLinksPreference(app_id);

  // Reinstall the app.
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessAlreadyInstalled);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Verify that the app is *not* the preferred app after re-installation as the
  // user may have already updated their preference.
  WaitForSupportedLinksPreference(app_id, /*is_preferred_app=*/false);
}

}  // namespace web_app
