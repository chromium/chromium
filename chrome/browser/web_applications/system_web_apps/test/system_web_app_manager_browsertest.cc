// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "components/permissions/permission_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_pref_names.h"
#endif

namespace {

// Helper to call AppServiceProxyFactory::GetForProfile().
apps::AppServiceProxyBase* GetAppServiceProxy(Profile* profile) {
  // Crash if there is no AppService support for |profile|. GetForProfile() will
  // DumpWithoutCrashing, which will not fail a test. No codepath should trigger
  // that in normal operation.
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  return apps::AppServiceProxyFactory::GetForProfile(profile);
}

}  // namespace

namespace web_app {

// Test that System Apps install correctly with a manifest.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest, Install) {
  WaitForTestSystemAppInstall();

  // Don't wait for page load because we want to verify AppController identifies
  // the System Web App before when the app loads.
  Browser* app_browser;
  LaunchAppWithoutWaiting(GetMockAppType(), &app_browser);

  AppId app_id = app_browser->app_controller()->GetAppId();
  EXPECT_EQ(GetManager().GetAppIdForSystemApp(GetMockAppType()), app_id);
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  Profile* profile = app_browser->profile();
  AppRegistrar& registrar =
      WebAppProviderBase::GetProviderBase(profile)->registrar();

  EXPECT_EQ("Test System App", registrar.GetAppShortName(app_id));
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0), registrar.GetAppThemeColor(app_id));
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, ExternalInstallSource::kSystemInstalled));
  EXPECT_EQ(
      registrar.FindAppWithUrlInScope(content::GetWebUIURL("test-system-app/")),
      app_id);

  // OS Integration only relevant for Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::OptionalBool::kTrue, update.ShowInLauncher());
        EXPECT_EQ(apps::mojom::OptionalBool::kTrue, update.ShowInSearch());
        EXPECT_EQ(apps::mojom::OptionalBool::kFalse, update.ShowInManagement());
        EXPECT_EQ(apps::mojom::Readiness::kReady, update.Readiness());
      });
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Check the toolbar is not shown for system web apps for pages on the chrome://
// scheme but is shown off the chrome:// scheme.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest,
                       ToolbarVisibilityForSystemWebApp) {
  WaitForTestSystemAppInstall();

  // Don't wait for page load because we want to verify the toolbar is hidden
  // when the window first opens.
  Browser* app_browser;
  LaunchAppWithoutWaiting(GetMockAppType(), &app_browser);

  // In scope, the toolbar should not be visible.
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Because the first part of the url is on a different origin (settings vs.
  // foo) a toolbar would normally be shown. However, because settings is a
  // SystemWebApp and foo is served via chrome:// it is okay not to show the
  // toolbar.
  GURL out_of_scope_chrome_page(content::kChromeUIScheme +
                                std::string("://foo"));
  content::NavigateToURLBlockUntilNavigationsComplete(
      app_browser->tab_strip_model()->GetActiveWebContents(),
      out_of_scope_chrome_page, 1);
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Even though the url is secure it is not being served over chrome:// so a
  // toolbar should be shown.
  GURL off_scheme_page("https://example.com");
  content::NavigateToURLBlockUntilNavigationsComplete(
      app_browser->tab_strip_model()->GetActiveWebContents(), off_scheme_page,
      1);
  EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest, LaunchMetricsWork) {
  WaitForTestSystemAppInstall();

  base::HistogramTester histograms;

  content::TestNavigationObserver navigation_observer(
      maybe_installation_->GetAppUrl());
  navigation_observer.StartWatchingNewWebContents();

  LaunchSystemWebAppAsync(
      browser()->profile(), GetMockAppType(),
      {.launch_source = apps::mojom::LaunchSource::kFromAppListGrid});

  navigation_observer.Wait();
  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromAppListGrid", 1);
  histograms.ExpectUniqueSample("Apps.DefaultAppLaunch.FromAppListGrid", 39, 1);
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest,
                       LaunchMetricsWorkFromAppProxy) {
  WaitForTestSystemAppInstall();

  base::HistogramTester histograms;
  content::TestNavigationObserver navigation_observer(
      maybe_installation_->GetAppUrl());
  navigation_observer.StartWatchingNewWebContents();

  auto* proxy = GetAppServiceProxy(browser()->profile());

  proxy->Launch(GetManager().GetAppIdForSystemApp(GetMockAppType()).value(),
                ui::EventFlags::EF_NONE,
                apps::mojom::LaunchSource::kFromAppListGrid,
                apps::MakeWindowInfo(display::kDefaultDisplayId));
  navigation_observer.Wait();

  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromAppListGrid", 1);
  histograms.ExpectUniqueSample("Apps.DefaultAppLaunch.FromAppListGrid", 39, 1);
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest,
                       LaunchMetricsWorkWithIntent) {
  WaitForTestSystemAppInstall();

  base::HistogramTester histograms;
  content::TestNavigationObserver navigation_observer(
      maybe_installation_->GetAppUrl());
  navigation_observer.StartWatchingNewWebContents();

  auto* proxy = GetAppServiceProxy(browser()->profile());
  auto intent = apps::mojom::Intent::New();
  intent->action = apps_util::kIntentActionView;
  intent->mime_type = "text/plain";

  proxy->LaunchAppWithIntent(
      GetManager().GetAppIdForSystemApp(GetMockAppType()).value(),
      ui::EventFlags::EF_NONE, std::move(intent),
      apps::mojom::LaunchSource::kFromAppListGrid,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
  navigation_observer.Wait();

  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromAppListGrid", 1);
  histograms.ExpectUniqueSample("Apps.DefaultAppLaunch.FromAppListGrid", 39, 1);
}

// The helper methods in this class uses ExecuteScriptXXX instead of ExecJs and
// EvalJs because of some quirks surrounding origin trials and content security
// policies.
class SystemWebAppManagerFileHandlingBrowserTestBase
    : public SystemWebAppBrowserTestBase,
      public ::testing::WithParamInterface<SystemWebAppManagerTestParams> {
 public:
  using IncludeLaunchDirectory =
      TestSystemWebAppInstallation::IncludeLaunchDirectory;

  explicit SystemWebAppManagerFileHandlingBrowserTestBase(
      IncludeLaunchDirectory include_launch_directory)
      : SystemWebAppBrowserTestBase(/*install_mock=*/false) {
    scoped_feature_blink_api_.InitWithFeatures(
        {blink::features::kFileHandlingAPI}, {});

    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
            include_launch_directory);
  }

  content::WebContents* LaunchApp(
      const std::vector<base::FilePath> launch_files,
      bool wait_for_load = true) {
    apps::AppLaunchParams params = LaunchParamsForApp(GetMockAppType());
    params.source = apps::mojom::AppLaunchSource::kSourceChromeInternal;
    params.launch_files = launch_files;

    return SystemWebAppBrowserTestBase::LaunchApp(std::move(params));
  }

  content::WebContents* LaunchAppWithoutWaiting(
      const std::vector<base::FilePath> launch_files) {
    apps::AppLaunchParams params = LaunchParamsForApp(GetMockAppType());
    params.source = apps::mojom::AppLaunchSource::kSourceChromeInternal;
    params.launch_files = launch_files;

    return SystemWebAppBrowserTestBase::LaunchAppWithoutWaiting(
        std::move(params));
  }

  void GrantFileHandlingPermisson() {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetDefaultContentSetting(ContentSettingsType::FILE_HANDLING,
                                  CONTENT_SETTING_ALLOW);
  }

  // Must be called before WaitAndExposeLaunchParamsToWindow. This sets up the
  // promise used to wait for launchParam callback.
  bool PrepareToReceiveLaunchParams(content::WebContents* web_contents) {
    return content::ExecuteScript(
        web_contents,
        "window.launchParamsPromise = new Promise(resolve => {"
        "  window.resolveLaunchParamsPromise = resolve;"
        "});"
        "launchQueue.setConsumer(launchParams => {"
        "  window.resolveLaunchParamsPromise(launchParams);"
        "  window.resolveLaunchParamsPromise = null;"
        "});");
  }

  // Must be called after PrepareToReceiveLaunchParams. This method waits for
  // launchParams being received, the stores it to a |js_property_name| on JS
  // window object.
  bool WaitAndExposeLaunchParamsToWindow(
      content::WebContents* web_contents,
      const std::string js_property_name = "launchParams") {
    bool launch_params_received;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        content::JsReplace("window.launchParamsPromise.then(launchParams => {"
                           "  window[$1] = launchParams;"
                           "  domAutomationController.send(true);"
                           "});",
                           js_property_name),
        &launch_params_received));
    return launch_params_received;
  }

  std::string GetJsStatementValueAsString(content::WebContents* web_contents,
                                          std::string js_statement) {
    std::string str;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents, "domAutomationController.send( " + js_statement + ");",
        &str));
    return str;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_web_app_provider_type_;
  base::test::ScopedFeatureList scoped_feature_blink_api_;
};

class SystemWebAppManagerLaunchFilesBrowserTest
    : public SystemWebAppManagerFileHandlingBrowserTestBase {
 public:
  SystemWebAppManagerLaunchFilesBrowserTest()
      : SystemWebAppManagerFileHandlingBrowserTestBase(
            IncludeLaunchDirectory::kNo) {}
};

// Check launch files are passed to application.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchFilesBrowserTest,
                       LaunchFilesForSystemWebApp) {
  WaitForTestSystemAppInstall();
  GrantFileHandlingPermisson();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  // First launch.
  content::WebContents* web_contents = LaunchApp({temp_file_path});

  // Check the App is launched with the correct launch file.
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams1"));
  EXPECT_EQ(temp_file_path.BaseName().AsUTF8Unsafe(),
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams1.files[0].name"));

  // Second launch.
  base::FilePath temp_file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path2));

  // The second launch reuses the opened application. It should pass the
  // launchParams to the opened page, and return the same content::WebContents*.
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_EQ(web_contents, LaunchAppWithoutWaiting({temp_file_path2}));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams2"));

  // Second launch_files are correct.
  EXPECT_EQ(temp_file_path2.BaseName().AsUTF8Unsafe(),
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams2.files[0].name"));
}

// The helper methods in this class uses ExecuteScriptXXX instead of ExecJs and
// EvalJs because of some quirks surrounding origin trials and content security
// policies.
class SystemWebAppManagerLaunchDirectoryBrowserTest
    : public SystemWebAppManagerFileHandlingBrowserTestBase {
 public:
  SystemWebAppManagerLaunchDirectoryBrowserTest()
      : SystemWebAppManagerFileHandlingBrowserTestBase(
            IncludeLaunchDirectory::kYes) {}

  // Returns the content of |file_handle_or_promise| file handle.
  std::string ReadContentFromJsFileHandle(
      content::WebContents* web_contents,
      const std::string& file_handle_or_promise) {
    std::string js_file_content;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents,
        "Promise.resolve(" + file_handle_or_promise + ")" +
            ".then(async (fileHandle) => {"
            "  const file = await fileHandle.getFile();"
            "  const content = await file.text();"
            "  window.domAutomationController.send(content);"
            "});",
        &js_file_content));
    return js_file_content;
  }

  // Writes |content_to_write| to |file_handle_or_promise| file handle. Returns
  // whether JavaScript execution finishes.
  bool WriteContentToJsFileHandle(content::WebContents* web_contents,
                                  const std::string& file_handle_or_promise,
                                  const std::string& content_to_write) {
    bool file_written;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        content::JsReplace(
            "Promise.resolve(" + file_handle_or_promise + ")" +
                ".then(async (fileHandle) => {"
                "  const writable = await fileHandle.createWritable();"
                "  await writable.write($1);"
                "  await writable.close();"
                "  window.domAutomationController.send(true);"
                "});",
            content_to_write),
        &file_written));
    return file_written;
  }

  // Remove file by |file_name| from |dir_handle_or_promise| directory handle.
  // Returns whether JavaScript execution finishes.
  bool RemoveFileFromJsDirectoryHandle(content::WebContents* web_contents,
                                       const std::string& dir_handle_or_promise,
                                       const std::string& file_name) {
    bool file_removed;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        content::JsReplace("Promise.resolve(" + dir_handle_or_promise + ")" +
                               ".then(async (dir_handle) => {"
                               "  await dir_handle.removeEntry($1);"
                               "  domAutomationController.send(true);"
                               "});",
                           file_name),
        &file_removed));
    return file_removed;
  }

  std::string ReadFileContent(const base::FilePath& path) {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(path, &content));
    return content;
  }

  // Launch the App with |base_dir| and a file inside this directory, then test
  // SWA can 1) read and write to the launch file; 2) read and write to other
  // files inside the launch directory; 3) read and write to the launch
  // directory (i.e. list and delete files).
  void TestPermissionsForLaunchDirectory(const base::FilePath& base_dir) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Create the launch file, which stores 4 characters "test".
    base::FilePath launch_file_path;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(base_dir, &launch_file_path));
    ASSERT_TRUE(base::WriteFile(launch_file_path, "test"));

    // Launch the App.
    content::WebContents* web_contents = LaunchApp({launch_file_path});

    // Launch directories and files passed to system web apps should
    // automatically be granted write permission. Users should not get
    // permission prompts. So we auto deny them (if they show up).
    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

    // Wait for launchParams.
    EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
    EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents));

    // Check we can read and write to the launch file.
    std::string launch_file_js_handle = "window.launchParams.files[1]";
    EXPECT_EQ("test",
              ReadContentFromJsFileHandle(web_contents, launch_file_js_handle));
    EXPECT_TRUE(WriteContentToJsFileHandle(web_contents, launch_file_js_handle,
                                           "js_written"));
    EXPECT_EQ("js_written", ReadFileContent(launch_file_path));

    // Check we can read and write to a different file inside the directory.
    // Note, this also checks we can read the launch directory, using
    // directory_handle.getFileHandle().
    base::FilePath non_launch_file_path;
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(base_dir, &non_launch_file_path));
    ASSERT_TRUE(base::WriteFile(non_launch_file_path, "test2"));

    std::string non_launch_file_js_handle =
        content::JsReplace("window.launchParams.files[0].getFileHandle($1)",
                           non_launch_file_path.BaseName().AsUTF8Unsafe());
    EXPECT_EQ("test2", ReadContentFromJsFileHandle(web_contents,
                                                   non_launch_file_js_handle));
    EXPECT_TRUE(WriteContentToJsFileHandle(
        web_contents, non_launch_file_js_handle, "js_written2"));
    EXPECT_EQ("js_written2", ReadFileContent(non_launch_file_path));

    // Check the launch file can be deleted.
    std::string launch_dir_js_handle = "window.launchParams.files[0]";
    EXPECT_TRUE(RemoveFileFromJsDirectoryHandle(
        web_contents, launch_dir_js_handle,
        launch_file_path.BaseName().AsUTF8Unsafe()));
    EXPECT_FALSE(base::PathExists(launch_file_path));

    // Check the non-launch file can be deleted.
    EXPECT_TRUE(RemoveFileFromJsDirectoryHandle(
        web_contents, launch_dir_js_handle,
        non_launch_file_path.BaseName().AsUTF8Unsafe()));
    EXPECT_FALSE(base::PathExists(non_launch_file_path));

    // Check a file can be created.
    std::string new_file_js_handle = content::JsReplace(
        "window.launchParams.files[0].getFileHandle($1, {create:true})",
        "new_file");
    EXPECT_TRUE(WriteContentToJsFileHandle(web_contents, new_file_js_handle,
                                           "js_new_file"));
    EXPECT_EQ("js_new_file", ReadFileContent(base_dir.AppendASCII("new_file")));
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchDirectoryBrowserTest,
                       LaunchDirectoryForSystemWebApp) {
  WaitForTestSystemAppInstall();
  GrantFileHandlingPermisson();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  // First launch.
  content::WebContents* web_contents = LaunchApp({temp_file_path});
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams1"));

  // Check launch directory and launch files are correct.
  EXPECT_EQ("directory",
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams1.files[0].kind"));
  EXPECT_EQ(temp_directory.GetPath().BaseName().AsUTF8Unsafe(),
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams1.files[0].name"));
  EXPECT_EQ("file", GetJsStatementValueAsString(
                        web_contents, "window.launchParams1.files[1].kind"));
  EXPECT_EQ(temp_file_path.BaseName().AsUTF8Unsafe(),
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams1.files[1].name"));

  // Second launch.
  base::ScopedTempDir temp_directory2;
  ASSERT_TRUE(temp_directory2.CreateUniqueTempDir());
  base::FilePath temp_file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory2.GetPath(),
                                             &temp_file_path2));

  // The second launch reuses the opened application. It should pass the
  // launchParams to the opened page, and return the same content::WebContents*.
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_EQ(web_contents, LaunchAppWithoutWaiting({temp_file_path2}));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams2"));

  // Check the second launch directory and launch files are correct.
  EXPECT_EQ("directory",
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams2.files[0].kind"));
  EXPECT_EQ(temp_directory2.GetPath().BaseName().AsUTF8Unsafe(),
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams2.files[0].name"));
  EXPECT_EQ("file", GetJsStatementValueAsString(
                        web_contents, "window.launchParams2.files[1].kind"));
  EXPECT_EQ(temp_file_path2.BaseName().AsUTF8Unsafe(),
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams2.files[1].name"));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchDirectoryBrowserTest,
                       ReadWritePermissions_OrdinaryDirectory) {
  WaitForTestSystemAppInstall();
  GrantFileHandlingPermisson();

  // Test for ordinary directory.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  TestPermissionsForLaunchDirectory(temp_directory.GetPath());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchDirectoryBrowserTest,
                       ReadWritePermissions_SensitiveDirectory) {
  WaitForTestSystemAppInstall();
  GrantFileHandlingPermisson();

  // Test for sensitive directory (which are otherwise blocked by
  // FileSystemAccess API). It is safe to use |chrome::DIR_DEFAULT_DOWNLOADS|,
  // because InProcBrowserTest fixture sets up different download directory for
  // each test cases.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath sensitive_dir;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &sensitive_dir));
  ASSERT_TRUE(base::DirectoryExists(sensitive_dir));
  TestPermissionsForLaunchDirectory(sensitive_dir);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Base class for testing File Handling and File System Access with Chrome OS
// File System Provider features.
class SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest
    : public SystemWebAppManagerLaunchDirectoryBrowserTest {
 public:
  bool CheckFileIsGif(content::WebContents* web_contents,
                      const std::string& file_handle_or_promise) {
    bool is_gif_signature;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "Promise.resolve(" + file_handle_or_promise + ")" +
            ".then(async file => {"
            "  const arrayBuf = await file.arrayBuffer();"
            "  const bytes = new Uint8Array(arrayBuf.slice(0, 3));"
            "  const isGifSignature = bytes[0] === 0x47        /* G */"
            "                         && bytes[1] === 0x49     /* I */ "
            "                         && bytes[2] === 0x46;    /* F */"
            "  domAutomationController.send(isGifSignature);"
            "});",
        &is_gif_signature));
    return is_gif_signature;
  }

  bool CheckFileIsPng(content::WebContents* web_contents,
                      const std::string& file_handle_or_promise) {
    bool is_png_signature;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "Promise.resolve(" + file_handle_or_promise + ")" +
            ".then(async file => {"
            "  const arrayBuf = await file.arrayBuffer();"
            "  const bytes = new Uint8Array(arrayBuf.slice(0, 4));"
            "  const isPngSignature = bytes[0] === 0x89        /* 0x89 */"
            "                         && bytes[1] === 0x50     /* P */"
            "                         && bytes[2] === 0x4E     /* N */"
            "                         && bytes[3] === 0x47;    /* G */"
            "  domAutomationController.send(isPngSignature);"
            "});",
        &is_png_signature));
    return is_png_signature;
  }

  // Returns whether the file is written.
  bool CheckCanWriteFile(content::WebContents* web_contents,
                         const std::string& file_handle_or_promise) {
    bool file_written;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "Promise.resolve(" + file_handle_or_promise + ")" +
            ".then(async fileHandle => {"
            "  try {"
            "    const writable = await fileHandle.createWritable();"
            "    await writable.write('test');"
            "    await writable.close();"
            "    domAutomationController.send(true);"
            "  } catch(err) {"
            "    console.error('write failed: ' + err.message);"
            "    domAutomationController.send(false);"
            "  }"
            "});",
        &file_written));
    return file_written;
  }

  void InstallTestFileSystemProvider(Profile* profile) {
    volume_ = file_manager::test::InstallFileSystemProviderChromeApp(profile);
  }

  base::FilePath GetFileSystemProviderFilePath(const std::string& file_name) {
    return volume_->mount_path().AppendASCII(file_name);
  }

 private:
  base::WeakPtr<file_manager::Volume> volume_;
};

IN_PROC_BROWSER_TEST_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest,
    LaunchFromFileSystemProvider_ReadFiles) {
  Profile* profile = browser()->profile();

  WaitForTestSystemAppInstall();
  GrantFileHandlingPermisson();
  InstallTestFileSystemProvider(profile);

  // Launch from FileSystemProvider path.
  const char kTestGifFile[] = "readwrite.gif";
  const char kTestPngFile[] = "readonly.png";
  const base::FilePath launch_file =
      GetFileSystemProviderFilePath(kTestGifFile);

  content::WebContents* web_contents = LaunchApp({launch_file});
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams"));

  // Check the launch file is the one we expect, and we can read the file.
  EXPECT_EQ(kTestGifFile,
            GetJsStatementValueAsString(web_contents,
                                        "window.launchParams.files[1].name"));
  EXPECT_TRUE(
      CheckFileIsGif(web_contents, "window.launchParams.files[1].getFile()"));

  // Check we can list the directory.
  std::string file_names;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "(async function() {"
      "  let fileNames = [];"
      "  const files = await window.launchParams.files[0].keys();"
      "  for await (const name of files)"
      "    fileNames.push(name);"
      "  domAutomationController.send(fileNames.sort().join(';'));"
      "})();",
      &file_names));
  EXPECT_EQ(base::StrCat({kTestPngFile, ";", kTestGifFile}), file_names);

  // Verify we can read a file (other than launch file) inside the directory.
  EXPECT_TRUE(CheckFileIsPng(
      web_contents,
      content::JsReplace("window.launchParams.files[0].getFileHandle($1).then("
                         "  fileHandle => fileHandle.getFile())",
                         kTestPngFile)));
}

// Test that the File System Access implementation doesn't cause a crash when
// writing to readonly files.
IN_PROC_BROWSER_TEST_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest,
    LaunchFromFileSystemProvider_WriteFileFails) {
  Profile* profile = browser()->profile();

  WaitForTestSystemAppInstall();
  GrantFileHandlingPermisson();
  InstallTestFileSystemProvider(profile);

  content::WebContents* web_contents =
      LaunchApp({GetFileSystemProviderFilePath("readonly.png")});

  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams"));

  // Try to write the file.
  EXPECT_FALSE(CheckCanWriteFile(web_contents, "window.launchParams.files[1]"));

  // Do a no-op JavaScript to check the page is still operational. If the page
  // crashed, the following call will fail.
  EXPECT_TRUE(content::ExecuteScript(web_contents, "(function() {})();"));
}

// Test that the File System Access implementation doesn't cause a crash when
// deleting readonly files.
IN_PROC_BROWSER_TEST_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest,
    LaunchFromFileSystemProvider_DeleteFileFails) {
  Profile* profile = browser()->profile();

  WaitForTestSystemAppInstall();
  GrantFileHandlingPermisson();
  InstallTestFileSystemProvider(profile);

  content::WebContents* web_contents =
      LaunchApp({GetFileSystemProviderFilePath("readonly.png")});

  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams"));

  // Try to delete the file.
  bool file_deleted;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      content::JsReplace("window.launchParams.files[0].removeEntry($1)"
                         ".then("
                         "  _ => domAutomationController.send(true),"
                         "  error => domAutomationController.send(false)"
                         ");",
                         "readonly.png"),
      &file_deleted));
  EXPECT_FALSE(file_deleted);

  // Do a no-op JavaScript to check the page is still operational. If the page
  // crashed, the following call will fail.
  EXPECT_TRUE(content::ExecuteScript(web_contents, "(function() {})();"));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class SystemWebAppManagerFileHandlingOriginTrialsBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerFileHandlingOriginTrialsBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
            OriginTrialsMap({{GetOrigin(GURL("chrome://test-system-app/")),
                              {"FileHandling"}}}));
  }

  ~SystemWebAppManagerFileHandlingOriginTrialsBrowserTest() override = default;

  content::WebContents* LaunchWithTestFiles() {
    // Grant permission.
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetDefaultContentSetting(ContentSettingsType::FILE_HANDLING,
                                  CONTENT_SETTING_ALLOW);

    // Create temporary directory and files.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir temp_directory;
    CHECK(temp_directory.CreateUniqueTempDir());
    base::FilePath temp_file_path;
    CHECK(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                         &temp_file_path));

    // Launch the App.
    apps::AppLaunchParams params = LaunchParamsForApp(GetMockAppType());
    params.source = apps::mojom::AppLaunchSource::kSourceChromeInternal;
    params.launch_files = {temp_file_path};

    return SystemWebAppBrowserTestBase::LaunchApp(std::move(params));
  }

  bool WaitForLaunchParam(content::WebContents* web_contents) {
    bool promise_resolved = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "launchQueue.setConsumer(launchParams => {"
        "  domAutomationController.send(true);"
        "});",
        &promise_resolved));
    return promise_resolved;
  }

 private:
  url::Origin GetOrigin(const GURL& url) { return url::Origin::Create(url); }
};

// Test that file handling works when the App is first installed.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerFileHandlingOriginTrialsBrowserTest,
                       PRE_FileHandlingWorks) {
  WaitForTestSystemAppInstall();

  content::WebContents* web_contents = LaunchWithTestFiles();
  EXPECT_TRUE(WaitForLaunchParam(web_contents));
}

// Test that file handling works when after a version upgrade.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerFileHandlingOriginTrialsBrowserTest,
                       FileHandlingWorks) {
  WaitForTestSystemAppInstall();

  content::WebContents* web_contents = LaunchWithTestFiles();
  EXPECT_TRUE(WaitForLaunchParam(web_contents));
}

class SystemWebAppManagerNotShownInLauncherTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerNotShownInLauncherTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppNotShownInLauncher();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerNotShownInLauncherTest,
                       NotShownInLauncher) {
  WaitForTestSystemAppInstall();

  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  // OS Integration only relevant for Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::OptionalBool::kFalse, update.ShowInLauncher());
      });
  // The |AppList| should have all apps visible in the launcher, apps get
  // removed from the |AppList| when they are hidden.
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  const ChromeAppListItem* mock_app = model_updater->FindItem(app_id);
  // |mock_app| shouldn't be found in |AppList| because it should be hidden in
  // launcher.
  EXPECT_FALSE(mock_app);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

class SystemWebAppManagerNotShownInSearchTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerNotShownInSearchTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppNotShownInSearch();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerNotShownInSearchTest,
                       NotShownInSearch) {
  WaitForTestSystemAppInstall();
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  // OS Integration only relevant for Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::OptionalBool::kFalse, update.ShowInSearch());
      });
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

class SystemWebAppManagerAdditionalSearchTermsTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerAdditionalSearchTermsTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerAdditionalSearchTermsTest,
                       AdditionalSearchTerms) {
  WaitForTestSystemAppInstall();
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  // AdditionalSearchTerms is flaky on Windows as it's a Chrome OS feature.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(std::vector<std::string>({"Security"}),
                  update.AdditionalSearchTerms());
      });
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Tests that SWA are correctly uninstalled across restarts.
class SystemWebAppManagerUninstallBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerUninstallBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    if (content::IsPreTest()) {
      // Use an app with FileHandling enabled since it will perform extra setup
      // steps.
      maybe_installation_ =
          TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
              OriginTrialsMap(
                  {{url::Origin::Create(GURL("chrome://test-system-app/")),
                    {"FileHandling"}}}));
    } else {
      maybe_installation_ = TestSystemWebAppInstallation::SetUpWithoutApps();
    }
  }
  ~SystemWebAppManagerUninstallBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerUninstallBrowserTest, PRE_Uninstall) {
  WaitForTestSystemAppInstall();
  EXPECT_TRUE(GetManager().GetAppIdForSystemApp(GetMockAppType()).has_value());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerUninstallBrowserTest, Uninstall) {
  WaitForTestSystemAppInstall();
  EXPECT_TRUE(GetManager().GetAppIds().empty());
}

// We only have concrete System Web Apps on Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// Test that all registered System Apps can be re-installed.
class SystemWebAppManagerInstallAllAppsBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerInstallAllAppsBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    features_.InitAndEnableFeature(features::kEnableAllSystemWebApps);
  }
  ~SystemWebAppManagerInstallAllAppsBrowserTest() override = default;

  // Don't use WaitForTestSystemAppInstall in this test, because it artificially
  // resets the OnAppsSynchronized signal, and starts a new synchronize request.
  void WaitForSystemAppsSynchronized() {
    base::RunLoop run_loop;
    WebAppProvider::Get(browser()->profile())
        ->system_web_app_manager()
        .on_apps_synchronized()
        .Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList features_;
};

// TODO(https://crbug.com/1162992): At the moment, PRE_Test failures aren't
// reported in test summary, thus won't fail the CI build job. So we need a
// ordinary test to fail the job and block CQ.
//
// Technically speaking, this test can merge into PRE_Upgrade if the
// aforementioned crbug is fixed.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerInstallAllAppsBrowserTest,
                       WebAppProtoEntryDefined) {
  const auto& app_map = GetManager().GetRegisteredSystemAppsForTesting();
  ASSERT_GT(app_map.size(), 0U);

  // Check all system app types has a corresponding SystemWebAppDataProto entry
  // defined.
  for (const auto& type_and_info : app_map) {
    EXPECT_TRUE(SystemWebAppDataProto_SystemAppType_IsValid(
        static_cast<SystemWebAppDataProto_SystemAppType>(type_and_info.first)))
        << "Please make sure you have added a corresponding entry to "
           "SystemWebAppDataProto when adding a new System Web App.";
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerInstallAllAppsBrowserTest,
                       PRE_Upgrade) {
  WaitForSystemAppsSynchronized();
  EXPECT_GE(GetManager().GetRegisteredSystemAppsForTesting().size(),
            GetManager().GetAppIds().size());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerInstallAllAppsBrowserTest, Upgrade) {
  WaitForSystemAppsSynchronized();
  const auto& app_ids = GetManager().GetAppIds();

  EXPECT_EQ(GetManager().GetRegisteredSystemAppsForTesting().size(),
            app_ids.size());

  for (const auto& app_id : app_ids) {
    const auto type = GetManager().GetSystemAppTypeForAppId(app_id).value();

    // We don't launch Terminal in browsertest, because it requires resources
    // that are only available in Chrome OS images.
    if (type == SystemAppType::TERMINAL)
      continue;

    // Launch other System Apps normally, and check the app's launch_url loads.
    EXPECT_TRUE(LaunchApp(type));
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that SWA-specific data is correctly migrated to Web Apps without
// Extensions.
class SystemWebAppManagerMigrationTest : public SystemWebAppBrowserTestBase {
 public:
  SystemWebAppManagerMigrationTest()
      : SystemWebAppBrowserTestBase(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms();
    maybe_installation_->set_update_policy(
        SystemWebAppManager::UpdatePolicy::kOnVersionChange);

    if (content::IsPreTest()) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }
  ~SystemWebAppManagerMigrationTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// These tests use the App Service which is only enabled on Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_PRE_ExtraDataIsMigrated PRE_ExtraDataIsMigrated
#else
#define MAYBE_PRE_ExtraDataIsMigrated DISABLED_PRE_ExtraDataIsMigrated
#endif
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerMigrationTest,
                       MAYBE_PRE_ExtraDataIsMigrated) {
  WaitForTestSystemAppInstall();
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  const bool app_found =
      GetAppServiceProxy(browser()->profile())
          ->AppRegistryCache()
          .ForOneApp(app_id, [](const apps::AppUpdate& update) {
            EXPECT_EQ(std::vector<std::string>({"Security"}),
                      update.AdditionalSearchTerms());
            EXPECT_EQ(apps::mojom::OptionalBool::kFalse,
                      update.ShowInManagement());
          });
  ASSERT_TRUE(app_found);
}

// These tests use the App Service which is only enabled on Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ExtraDataIsMigrated ExtraDataIsMigrated
#else
#define MAYBE_ExtraDataIsMigrated DISABLED_ExtraDataIsMigrated
#endif
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerMigrationTest,
                       MAYBE_ExtraDataIsMigrated) {
  WaitForTestSystemAppInstall();
  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();

  auto* proxy = GetAppServiceProxy(browser()->profile());
  const bool app_found = proxy->AppRegistryCache().ForOneApp(
      app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(std::vector<std::string>({"Security"}),
                  update.AdditionalSearchTerms());
        EXPECT_EQ(apps::mojom::OptionalBool::kFalse, update.ShowInManagement());
      });
  ASSERT_TRUE(app_found);
}

class SystemWebAppManagerChromeUntrustedTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerChromeUntrustedTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpChromeUntrustedApp();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerChromeUntrustedTest, Install) {
  WaitForTestSystemAppInstall();

  // Don't wait for page load because we want to verify AppController identifies
  // the System Web App before the app loads.
  Browser* app_browser;
  LaunchAppWithoutWaiting(GetMockAppType(), &app_browser);

  AppId app_id = GetManager().GetAppIdForSystemApp(GetMockAppType()).value();
  EXPECT_EQ(app_id, app_browser->app_controller()->GetAppId());
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  Profile* profile = app_browser->profile();
  AppRegistrar& registrar =
      WebAppProviderBase::GetProviderBase(profile)->registrar();

  EXPECT_EQ("Test System App", registrar.GetAppShortName(app_id));
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0), registrar.GetAppThemeColor(app_id));
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, ExternalInstallSource::kSystemInstalled));
  EXPECT_EQ(registrar.FindAppWithUrlInScope(
                GURL("chrome-untrusted://test-system-app/")),
            app_id);
}

class SystemWebAppManagerOriginTrialsBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerOriginTrialsBrowserTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
            OriginTrialsMap({{GetOrigin(main_url_), main_url_trials_},
                             {GetOrigin(trial_url_), trial_url_trials_}}));
  }

  ~SystemWebAppManagerOriginTrialsBrowserTest() override = default;

 protected:
  class MockNavigationHandle : public content::MockNavigationHandle {
   public:
    explicit MockNavigationHandle(const GURL& url)
        : content::MockNavigationHandle(url, nullptr) {}
    bool IsInMainFrame() override { return is_in_main_frame_; }

    void set_is_in_main_frame(bool is_in_main_frame) {
      is_in_main_frame_ = is_in_main_frame;
    }

   private:
    bool is_in_main_frame_;
  };

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    content::WebContents::CreateParams create_params(browser()->profile());
    return content::WebContents::Create(create_params);
  }

  const std::vector<std::string> main_url_trials_ = {"Frobulate"};
  const std::vector<std::string> trial_url_trials_ = {"FrobulateNavigation"};

  const GURL main_url_ = GURL("chrome://test-system-app/pwa.html");
  const GURL trial_url_ = GURL("chrome://test-subframe/title2.html");
  const GURL notrial_url_ = GURL("chrome://notrial-subframe/title3.html");

 private:
  url::Origin GetOrigin(const GURL& url) { return url::Origin::Create(url); }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_FirstNavigationIntoPage) {
  WaitForTestSystemAppInstall();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  WebAppTabHelper tab_helper(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate loading app's embedded child-frame that has origin trials.
  {
    MockNavigationHandle mock_nav_handle(trial_url_);
    mock_nav_handle.set_is_in_main_frame(false);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(trial_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }

  // Simulate loading app's embedded child-frame that has no origin trial.
  {
    MockNavigationHandle mock_nav_handle(notrial_url_);
    mock_nav_handle.set_is_in_main_frame(false);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_IntraDocumentNavigation) {
  WaitForTestSystemAppInstall();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  WebAppTabHelper tab_helper(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate same-document navigation.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(true);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }
}

// This test checks origin trials are correctly enabled for navigations on the
// main frame, this test checks:
// - The app's main page |main_url_| has OT.
// - The iframe page |trial_url_| has OT, only if it is embedded by the app.
// - When navigating from a cross-origin page to the app's main page, the main
// page has OT.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_Navigation) {
  WaitForTestSystemAppInstall();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  WebAppTabHelper tab_helper(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate navigating to a different site without origin trials.
  {
    MockNavigationHandle mock_nav_handle(notrial_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ("", tab_helper.GetAppId());
  }

  // Simulatenavigating back to a SWA with origin trials.
  {
    MockNavigationHandle mock_nav_handle(main_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(maybe_installation_->GetAppId(), tab_helper.GetAppId());
  }

  // Simulate navigating the main frame to a url embedded by SWA. This url has
  // origin trials when embedded by SWA. However, when this url is loaded in the
  // main frame, it should not get origin trials.
  {
    MockNavigationHandle mock_nav_handle(trial_url_);
    mock_nav_handle.set_is_in_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ("", tab_helper.GetAppId());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class SystemWebAppManagerAppSuspensionBrowserTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerAppSuspensionBrowserTest()
      : SystemWebAppManagerBrowserTest(false) {}

  apps::mojom::Readiness GetAppReadiness(const AppId& app_id) {
    apps::mojom::Readiness readiness;
    bool app_found =
        GetAppServiceProxy(browser()->profile())
            ->AppRegistryCache()
            .ForOneApp(app_id, [&readiness](const apps::AppUpdate& update) {
              readiness = update.Readiness();
            });
    CHECK(app_found);
    return readiness;
  }

  apps::mojom::IconKeyPtr GetAppIconKey(const AppId& app_id) {
    apps::mojom::IconKeyPtr icon_key;
    bool app_found =
        GetAppServiceProxy(browser()->profile())
            ->AppRegistryCache()
            .ForOneApp(app_id, [&icon_key](const apps::AppUpdate& update) {
              icon_key = update.IconKey();
            });
    CHECK(app_found);
    return icon_key;
  }
};

// Tests that System Apps can be suspended when the policy is set before the app
// is installed.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerAppSuspensionBrowserTest,
                       AppSuspendedBeforeInstall) {
  ASSERT_FALSE(
      GetManager().GetAppIdForSystemApp(SystemAppType::SETTINGS).has_value());
  {
    ListPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                          policy::policy_prefs::kSystemFeaturesDisableList);
    base::ListValue* list = update.Get();
    list->Append(policy::SystemFeature::kOsSettings);
  }
  WaitForTestSystemAppInstall();
  base::Optional<AppId> settings_id =
      GetManager().GetAppIdForSystemApp(SystemAppType::SETTINGS);
  DCHECK(settings_id.has_value());

  EXPECT_EQ(apps::mojom::Readiness::kDisabledByPolicy,
            GetAppReadiness(*settings_id));
  EXPECT_TRUE(apps::IconEffects::kBlocked &
              GetAppIconKey(*settings_id)->icon_effects);

  {
    ListPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                          policy::policy_prefs::kSystemFeaturesDisableList);
    base::ListValue* list = update.Get();
    list->Clear();
  }
  GetAppServiceProxy(browser()->profile())->FlushMojoCallsForTesting();
  EXPECT_EQ(apps::mojom::Readiness::kReady, GetAppReadiness(*settings_id));
  EXPECT_FALSE(apps::IconEffects::kBlocked &
               GetAppIconKey(*settings_id)->icon_effects);
}

// Tests that System Apps can be suspended when the policy is set after the app
// is installed.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerAppSuspensionBrowserTest,
                       AppSuspendedAfterInstall) {
  WaitForTestSystemAppInstall();
  base::Optional<AppId> settings_id =
      GetManager().GetAppIdForSystemApp(SystemAppType::SETTINGS);
  DCHECK(settings_id.has_value());
  EXPECT_EQ(apps::mojom::Readiness::kReady, GetAppReadiness(*settings_id));

  {
    ListPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                          policy::policy_prefs::kSystemFeaturesDisableList);
    base::ListValue* list = update.Get();
    list->Append(policy::SystemFeature::kOsSettings);
  }

  auto* proxy = GetAppServiceProxy(browser()->profile());
  proxy->FlushMojoCallsForTesting();
  EXPECT_EQ(apps::mojom::Readiness::kDisabledByPolicy,
            GetAppReadiness(*settings_id));
  EXPECT_TRUE(apps::IconEffects::kBlocked &
              GetAppIconKey(*settings_id)->icon_effects);

  {
    ListPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                          policy::policy_prefs::kSystemFeaturesDisableList);
    base::ListValue* list = update.Get();
    list->Clear();
  }
  proxy->FlushMojoCallsForTesting();
  EXPECT_EQ(apps::mojom::Readiness::kReady, GetAppReadiness(*settings_id));
  EXPECT_FALSE(apps::IconEffects::kBlocked &
               GetAppIconKey(*settings_id)->icon_effects);
}
// This feature will only work when DesktopPWAsWithoutExtensions launches.
INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerAppSuspensionBrowserTest);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class SystemWebAppManagerBackgroundTaskTest
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerBackgroundTaskTest()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        TestSystemWebAppInstallation::SetUpAppWithBackgroundTask();
  }

  void WaitForSystemAppsBackgroundTasksStart() {
    base::RunLoop run_loop;
    WebAppProvider::Get(browser()->profile())
        ->system_web_app_manager()
        .on_tasks_started()
        .Post(FROM_HERE, run_loop.QuitClosure());

    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBackgroundTaskTest, TimerFires) {
  // The SystemWebAppManager gets created in the Setup(), in the test
  // constructor, and the background tasks get created during synchronize.
  // Ideally, we'd make a TestNavigationObserver in the constructor, but they
  // have to be single threaded, and throw a check fail. There's a race
  // condition here because the background tasks are fired as callbacks in
  // response to the install finishing. So, we wait for the apps to be
  // installed, then wait on the navigation. A cleaner solution would be to have
  // a hook in the background pages to detect the navigation as an event. That's
  // a little too much work for one test though, and since this is mostly tested
  // in unittests, this is probably enough.
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://test-system-app/page2.html"));
  navigation_observer.StartWatchingNewWebContents();

  WaitForSystemAppsBackgroundTasksStart();

  auto& tasks = GetManager().GetBackgroundTasksForTesting();
  auto* start_timer = tasks[0]->get_start_timer_for_testing();
  EXPECT_EQ(base::TimeDelta::FromSeconds(120), start_timer->GetCurrentDelay());
  EXPECT_EQ(base::TimeDelta::FromDays(1),
            tasks[0]->get_repeating_timer_for_testing()->GetCurrentDelay());

  // The "Immediate" timer waits for 2 minutes, and it's really hard to mock
  // time properly in a browser test, so just fire the thing now. We're not
  // testing that base::Timer works.
  start_timer->FireNow();

  navigation_observer.Wait();
  EXPECT_FALSE(start_timer->IsRunning());
  EXPECT_EQ(1u, tasks.size());
  EXPECT_TRUE(tasks[0]->open_immediately_for_testing());
  EXPECT_EQ(base::TimeDelta::FromDays(1), tasks[0]->period_for_testing());
  EXPECT_EQ(1u, tasks[0]->timer_activated_count_for_testing());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerLaunchFilesBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerLaunchDirectoryBrowserTest);

#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest);
#endif

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerNotShownInLauncherTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerNotShownInSearchTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerAdditionalSearchTermsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerChromeUntrustedTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerOriginTrialsBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerFileHandlingOriginTrialsBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerUninstallBrowserTest);

#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerInstallAllAppsBrowserTest);
#endif

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerBackgroundTaskTest);

}  // namespace web_app
