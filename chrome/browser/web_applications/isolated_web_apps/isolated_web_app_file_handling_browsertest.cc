// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/commands/launch_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#endif

namespace web_app {

class IsolatedWebAppFileHandlingBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  WebAppFileHandlerManager& file_handler_manager() {
    return WebAppProvider::GetForTest(profile())
        ->os_integration_manager()
        .file_handler_manager();
  }

  webapps::AppId InstallFileHandlingIwa() {
    return IsolatedWebAppBuilder(
               ManifestBuilder().AddFileHandler("/", {{"text/*", {".txt"}}}))
        .BuildBundle()
        ->TrustBundleAndInstall(profile())
        ->app_id();
  }

  // Launches the |app_id| web app with |files| handles and runs a callback.
  content::WebContents* LaunchWithFiles(
      const webapps::AppId& app_id,
      const std::vector<base::FilePath>& files) {
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);

    auto launch_infos =
        file_handler_manager().GetMatchingFileHandlerUrls(app_id, files);
    EXPECT_EQ(1u, launch_infos.size());

    const auto& [url, launch_files] = launch_infos[0];
    params.launch_files = launch_files;
    params.override_url = url;

    content::WebContents* web_contents =
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->BrowserAppLauncher()
            ->LaunchAppWithParamsForTesting(std::move(params));

    content::WaitForLoadStop(web_contents);

    return web_contents;
  }

  // Attach the launchParams to the window so we can inspect them easily.
  void AttachTestConsumer(content::WebContents* web_contents) {
    ASSERT_TRUE(ExecJs(web_contents, R"(
        launchQueue.setConsumer(launchParams => {
          window.launchParams = launchParams;
        }))"));
  }

  void VerifyIwaDidReceiveFileLaunchParams(
      content::WebContents* web_contents,
      const base::FilePath& expected_file_path) {
    ASSERT_EQ(true, EvalJs(web_contents, "!!window.launchParams"));
    EXPECT_EQ(1, EvalJs(web_contents, "window.launchParams.files.length"));
    EXPECT_EQ(expected_file_path.BaseName().AsUTF8Unsafe(),
              EvalJs(web_contents, "window.launchParams.files[0].name"));
    EXPECT_EQ("granted", EvalJs(web_contents, R"(
        window.launchParams.files[0].queryPermission({mode: 'readwrite'}))"));
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       CanReceiveFileLaunchParams) {
  base::FilePath test_file_path = CreateTestFileWithExtension("txt");

  auto app_id = InstallFileHandlingIwa();
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, {test_file_path});
  AttachTestConsumer(web_contents);

  VerifyIwaDidReceiveFileLaunchParams(web_contents, test_file_path);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       FileExtensionCaseInsensitive) {
  base::FilePath test_file_path = CreateTestFileWithExtension("TXT");

  auto app_id = InstallFileHandlingIwa();
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, {test_file_path});
  AttachTestConsumer(web_contents);

  VerifyIwaDidReceiveFileLaunchParams(web_contents, test_file_path);
}

#if BUILDFLAG(IS_CHROMEOS)
// End-to-end test to ensure the file handler is registered on ChromeOS when the
// extension system is initialized. Gives more coverage than the unit tests.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       IsFileHandlerOnChromeOS) {
  auto app_id = InstallFileHandlingIwa();

  base::FilePath test_file_path = CreateTestFileWithExtension("txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);
  // Note that there are normally multiple tasks due to default-installed
  // handlers (e.g. add to zip file). But those handlers are not installed by
  // default in browser tests.
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor.app_id, app_id);
}

// Ensures correct behavior for files on "special volumes", such as file systems
// provided by extensions. These do not have local files (i.e. backed by
// inodes).
IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       HandlerForNonNativeFiles) {
  auto app_id = InstallFileHandlingIwa();
  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(profile());

  // File in chrome/test/data/extensions/api_test/file_browser/image_provider/.
  base::FilePath test_file_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);
  // This test should work the same as IsFileHandlerOnChromeOS.
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor.app_id, app_id);
}
#endif

}  // namespace web_app
