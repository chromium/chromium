// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/web_application_info.h"
#include "components/embedder_support/switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_launch/file_handling_expiry.mojom-test-utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#endif

// A fake file handling expiry service. This service allows us to mock having an
// origin trial which expires having a certain time, without needing to manage
// actual origin trial tokens.
class FakeFileHandlingExpiryService
    : public blink::mojom::FileHandlingExpiryInterceptorForTesting {
 public:
  FakeFileHandlingExpiryService()
      : expiry_time_(base::Time::Now() + base::TimeDelta::FromDays(1)) {}

  blink::mojom::FileHandlingExpiry* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<blink::mojom::FileHandlingExpiry>(
            std::move(handle)));
  }

  void SetExpiryTime(base::Time expiry_time) { expiry_time_ = expiry_time; }

  void RequestOriginTrialExpiryTime(
      RequestOriginTrialExpiryTimeCallback callback) override {
    if (before_reply_callback_) {
      std::move(before_reply_callback_).Run();
    }

    std::move(callback).Run(expiry_time_);
  }

  // Set a callback to be called before FileHandlingExpiry interface replies
  // the expiry time. Useful for testing inflight IPC.
  void SetBeforeReplyCallback(base::RepeatingClosure before_reply_callback) {
    before_reply_callback_ = before_reply_callback;
  }

 private:
  base::Time expiry_time_;
  RequestOriginTrialExpiryTimeCallback callback_;
  base::RepeatingClosure before_reply_callback_;
  mojo::AssociatedReceiver<blink::mojom::FileHandlingExpiry> receiver_{this};
};

class WebAppFileHandlingTestBase : public web_app::WebAppControllerBrowserTest {
 public:
  web_app::WebAppProviderBase* provider() {
    return web_app::WebAppProviderBase::GetProviderBase(profile());
  }

  web_app::FileHandlerManager& file_handler_manager() {
    return provider()
        ->os_integration_manager()
        .file_handler_manager_for_testing();
  }

  web_app::AppRegistrar& registrar() { return provider()->registrar(); }

  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetTextFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/blank_page.html");
  }

  GURL GetCSVFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/page_with_refs.html");
  }

  void InstallFileHandlingPWA() {
    GURL url = GetSecureAppURL();

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->title = base::ASCIIToUTF16("A Hosted App");

    blink::Manifest::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.name = base::ASCIIToUTF16("text");
    entry1.accept[base::ASCIIToUTF16("text/*")].push_back(
        base::ASCIIToUTF16(".txt"));
    web_app_info->file_handlers.push_back(std::move(entry1));

    blink::Manifest::FileHandler entry2;
    entry2.action = GetCSVFileHandlerActionURL();
    entry2.name = base::ASCIIToUTF16("csv");
    entry2.accept[base::ASCIIToUTF16("application/csv")].push_back(
        base::ASCIIToUTF16(".csv"));
    web_app_info->file_handlers.push_back(std::move(entry2));

    app_id_ =
        WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

 protected:
  const web_app::AppId& app_id() { return app_id_; }

 private:
  web_app::AppId app_id_;
};

namespace {

base::FilePath NewTestFilePath(const base::FilePath::CharType* extension) {
  // CreateTemporaryFile blocks, temporarily allow blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // In order to test file handling, we need to be able to supply a file
  // extension for the temp file.
  base::FilePath test_file_path;
  base::CreateTemporaryFile(&test_file_path);
  base::FilePath new_file_path = test_file_path.AddExtension(extension);
  EXPECT_TRUE(base::ReplaceFile(test_file_path, new_file_path, nullptr));
  return new_file_path;
}

content::WebContents* LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    const GURL& expected_launch_url,
    const apps::mojom::LaunchContainer launch_container =
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
    const apps::mojom::AppLaunchSource launch_source =
        apps::mojom::AppLaunchSource::kSourceTest,
    const std::vector<base::FilePath>& files = std::vector<base::FilePath>()) {
  apps::AppLaunchParams params(app_id, launch_container,
                               WindowOpenDisposition::NEW_WINDOW,
                               launch_source);

  if (files.size())
    params.launch_files = files;

  content::TestNavigationObserver navigation_observer(expected_launch_url);
  navigation_observer.StartWatchingNewWebContents();

  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(params);

  navigation_observer.Wait();

  // Attach the launchParams to the window so we can inspect them easily.
  auto result = content::EvalJs(web_contents,
                                "launchQueue.setConsumer(launchParams => {"
                                "  window.launchParams = launchParams;"
                                "});");

  return web_contents;
}

}  // namespace

class WebAppFileHandlingBrowserTest : public WebAppFileHandlingTestBase {
 public:
  WebAppFileHandlingBrowserTest() {
    scoped_feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI},
                                          {});
  }
  content::WebContents* LaunchWithFiles(
      const std::string& app_id,
      const GURL& expected_launch_url,
      const std::vector<base::FilePath>& files,
      const apps::mojom::LaunchContainer launch_container =
          apps::mojom::LaunchContainer::kLaunchContainerWindow) {
    return LaunchApplication(
        profile(), app_id, expected_launch_url, launch_container,
        apps::mojom::AppLaunchSource::kSourceFileHandler, files);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       LaunchConsumerIsNotTriggeredWithNoFiles) {
  InstallFileHandlingPWA();
  content::WebContents* web_contents =
      LaunchWithFiles(app_id(), GetSecureAppURL(), {});
  EXPECT_EQ(false, content::EvalJs(web_contents, "!!window.launchParams"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParams) {
  InstallFileHandlingPWA();
  base::FilePath test_file_path = NewTestFilePath(FILE_PATH_LITERAL("txt"));
  content::WebContents* web_contents = LaunchWithFiles(
      app_id(), GetTextFileHandlerActionURL(), {test_file_path});

  EXPECT_EQ(1,
            content::EvalJs(web_contents, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().value(),
            content::EvalJs(web_contents, "window.launchParams.files[0].name"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParamsInTab) {
  InstallFileHandlingPWA();
  base::FilePath test_file_path = NewTestFilePath(FILE_PATH_LITERAL("txt"));
  content::WebContents* web_contents =
      LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path},
                      apps::mojom::LaunchContainer::kLaunchContainerTab);

  EXPECT_EQ(1,
            content::EvalJs(web_contents, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().value(),
            content::EvalJs(web_contents, "window.launchParams.files[0].name"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsDispatchOnCorrectFileHandlingURL) {
  InstallFileHandlingPWA();

  // Test that file handler dispatches correct URL based on file extension.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {});
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("txt"))});
  LaunchWithFiles(app_id(), GetCSVFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("csv"))});

  // Test as above in a tab.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("txt"))},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetCSVFileHandlerActionURL(),
                  {NewTestFilePath(FILE_PATH_LITERAL("csv"))},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
}

class WebAppFileHandlingOriginTrialBrowserTest
    : public WebAppFileHandlingTestBase {
 public:
  WebAppFileHandlingOriginTrialBrowserTest() {
    web_app::FileHandlerManager::DisableAutomaticFileHandlerCleanupForTesting();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUpOnMainThread() override {
    WebAppFileHandlingTestBase::SetUpOnMainThread();
  }

  void SetUpInterceptorNavigateToAppAndMaybeWait() {
    base::RunLoop loop;
    file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
        loop.QuitClosure());
    web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            blink::mojom::FileHandlingExpiry::Name_,
            base::BindRepeating(&FakeFileHandlingExpiryService::Bind,
                                base::Unretained(&file_handling_expiry_)));
    NavigateInRenderer(web_contents(), GetSecureAppURL());

    // The expiry time is only updated if the app is installed.
    if (registrar().IsInstalled(app_id()))
      loop.Run();
  }

 protected:
  FakeFileHandlingExpiryService& file_handling_expiry() {
    return file_handling_expiry_;
  }

 private:
  FakeFileHandlingExpiryService file_handling_expiry_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       FileHandlingIsNotAvailableUntilOriginTrialIsChecked) {
  InstallFileHandlingPWA();

  // We haven't navigated to the app, so we don't know if it's allowed to handle
  // files.
  EXPECT_FALSE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_FALSE(file_handler_manager().AreFileHandlersEnabled(app_id()));

  // Navigating to the app should update the origin trial expiry (and allow it
  // to handle files).
  SetUpInterceptorNavigateToAppAndMaybeWait();

  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       FileHandlingOriginTrialIsCheckedAtInstallation) {
  // Navigate to the app's launch url, so the origin trial token can be checked.
  SetUpInterceptorNavigateToAppAndMaybeWait();

  base::RunLoop loop;
  file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
      loop.QuitClosure());
  InstallFileHandlingPWA();
  loop.Run();

  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       WhenOriginTrialHasExpiredFileHandlersAreNotAvailable) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));

  // Set the token's expiry to some time in the past.
  file_handling_expiry().SetExpiryTime(base::Time());

  // Refresh the page, to receive the updated expiry time.
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_FALSE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  EXPECT_FALSE(file_handler_manager().GetEnabledFileHandlers(app_id()));
}

// Tests that expired file handlers are cleaned up.
// Part 1: Install a file handling app and set it's expiry time to some time in
// the past.
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       PRE_ExpiredTrialHandlersAreCleanedUpAtLaunch) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));

  // Update the expiry time to be in the past.
  web_app::UpdateDoubleWebAppPref(profile()->GetPrefs(), app_id(),
                                  web_app::kFileHandlingOriginTrialExpiryTime,
                                  base::Time().ToDoubleT());
}

// Part 2: Test that expired file handlers for an app are cleaned up.
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       ExpiredTrialHandlersAreCleanedUpAtLaunch) {
  EXPECT_EQ(1, file_handler_manager().TriggerFileHandlerCleanupForTesting());
}

// Tests that non expired file handlers are not cleaned up.
// Part 1: Install an app with valid file handlers.
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       PRE_ValidFileHandlerAreNotCleanedUpAtLaunch) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
}

// Part 2: Test that expired file handlers for an app are cleaned up.
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       ValidFileHandlerAreNotCleanedUpAtLaunch) {
  EXPECT_EQ(0, file_handler_manager().TriggerFileHandlerCleanupForTesting());
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       DisableForceEnabledFileHandlingOriginTrial) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();

  ASSERT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  ASSERT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));

  // Calling this on non-force-enabled origin trial should have no effect.
  file_handler_manager().DisableForceEnabledFileHandlingOriginTrial(app_id());
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  EXPECT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));

  // Force enables file handling.
  file_handler_manager().ForceEnableFileHandlingOriginTrial(app_id());

  // Calling this on force enabled origin trial should remove file handlers.
  file_handler_manager().DisableForceEnabledFileHandlingOriginTrial(app_id());
  EXPECT_FALSE(file_handler_manager().AreFileHandlersEnabled(app_id()));
  EXPECT_EQ(nullptr, file_handler_manager().GetEnabledFileHandlers(app_id()));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       ForceEnabledFileHandling_IgnoreExpiryTimeUpdate) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();

  EXPECT_FALSE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));

  // Force enables file handling.
  file_handler_manager().ForceEnableFileHandlingOriginTrial(app_id());
  EXPECT_TRUE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));

  // Update origin trial expiry time from the App's WebContents.
  base::RunLoop loop;
  file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
      loop.QuitClosure());
  file_handling_expiry().SetExpiryTime(base::Time());
  file_handler_manager().MaybeUpdateFileHandlingOriginTrialExpiry(
      web_contents(), app_id());
  loop.Run();

  // Force enabled file handling should not be updated by the expiry time in
  // App's WebContents (i.e. origin trial token expiry).
  EXPECT_TRUE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));
  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialBrowserTest,
                       ForceEnabledFileHandling_IgnoreExpiryTimeInflightIPC) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();

  EXPECT_FALSE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));

  // Request to update origin trial expiry time from the App's WebContents, and
  // force enables file handling origin trial before the expiry time reply is
  // received.
  base::RunLoop loop;
  file_handler_manager().SetOnFileHandlingExpiryUpdatedForTesting(
      loop.QuitClosure());
  file_handling_expiry().SetExpiryTime(base::Time());
  file_handling_expiry().SetBeforeReplyCallback(
      base::BindLambdaForTesting([&]() {
        EXPECT_FALSE(
            file_handler_manager().IsFileHandlingForceEnabled(app_id()));
        file_handler_manager().ForceEnableFileHandlingOriginTrial(app_id());
      }));

  EXPECT_FALSE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));
  file_handler_manager().MaybeUpdateFileHandlingOriginTrialExpiry(
      web_contents(), app_id());
  loop.Run();

  // Force enabled file handling should not be updated by the inflight expiry
  // time IPC.
  EXPECT_TRUE(file_handler_manager().IsFileHandlingForceEnabled(app_id()));
  EXPECT_TRUE(file_handler_manager().IsFileHandlingAPIAvailable(app_id()));
  EXPECT_TRUE(file_handler_manager().GetEnabledFileHandlers(app_id()));
}

namespace {
constexpr char kBaseDataDir[] = "chrome/test/data/web_app_file_handling";

// This is the public key of tools/origin_trials/eftest.key, used to validate
// origin trial tokens generated by tools/origin_trials/generate_token.py.
constexpr char kOriginTrialPublicKeyForTesting[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

}  // namespace

class WebAppFileHandlingOriginTrialTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebAppControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialPublicKeyForTesting);
  }

  void TearDownOnMainThread() override { interceptor_.reset(); }

 protected:
  web_app::AppId InstallFileHandlingWebApp(GURL* start_url_out = nullptr) {
    std::string origin = "https://file-handling-pwa";

    // We need to use URLLoaderInterceptor (rather than a EmbeddedTestServer),
    // because origin trial token is associated with a fixed origin, whereas
    // EmbeddedTestServer serves content on a random port.
    interceptor_ =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            kBaseDataDir, GURL(origin));

    GURL start_url = GURL(origin + "/index.html");

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = base::ASCIIToUTF16("A Web App");

    blink::Manifest::FileHandler entry1;
    entry1.action = start_url;
    entry1.name = base::ASCIIToUTF16("text");
    entry1.accept[base::ASCIIToUTF16("text/*")].push_back(
        base::ASCIIToUTF16(".txt"));
    web_app_info->file_handlers.push_back(std::move(entry1));

    web_app::AppId app_id =
        WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));

    // Here we need first launch the App, so it can update the origin trial
    // expiry time in prefs. This is needed because the above InstallWebApp
    // invocation bypassed the normal Web App install pipeline.
    content::WebContents* web_content =
        LaunchApplication(profile(), app_id, start_url);
    web_content->Close();

    if (start_url_out)
      *start_url_out = start_url;
    return app_id;
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialTest,
                       LaunchParamsArePassedCorrectly) {
  GURL start_url;
  const web_app::AppId app_id = InstallFileHandlingWebApp(&start_url);
  base::FilePath test_file_path = NewTestFilePath(FILE_PATH_LITERAL("txt"));
  content::WebContents* web_content = LaunchApplication(
      profile(), app_id, start_url,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      apps::mojom::AppLaunchSource::kSourceFileHandler, {test_file_path});
  EXPECT_EQ(1,
            content::EvalJs(web_content, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_content, "window.launchParams.files[0].name"));
}

#if defined(OS_CHROMEOS)

// End-to-end test to ensure the file handler is registered on ChromeOS when the
// extension system is initialized. Gives more coverage than the unit tests for
// web_file_tasks.cc.
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialTest,
                       IsFileHandlerOnChromeOS) {
  const web_app::AppId app_id = InstallFileHandlingWebApp();
  base::FilePath test_file_path = NewTestFilePath(FILE_PATH_LITERAL("txt"));
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);

  // Note that there are normally multiple tasks due to default-installed
  // handlers (e.g. add to zip file). But those handlers are not installed by
  // default in browser tests.
  EXPECT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor().app_id, app_id);
}

// Ensures correct behavior for files on "special volumes", such as file systems
// provided by extensions. These do not have local files (i.e. backed by
// inodes).
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingOriginTrialTest,
                       NotHandlerForNonNativeFiles) {
  const web_app::AppId app_id = InstallFileHandlingWebApp();
  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(profile());

  // File in chrome/test/data/extensions/api_test/file_browser/image_provider/.
  base::FilePath test_file_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);

  // Current expectation is for the task not to be found while the native
  // filesystem API is still being built up. See https://crbug.com/1079065.
  // When the "special file" check in file_manager::file_tasks::FindWebTasks()
  // is removed, this test should work the same as IsFileHandlerOnChromeOS.
  EXPECT_EQ(0u, tasks.size());
}

#endif  // OS_CHROMEOS

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppFileHandlingBrowserTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppFileHandlingOriginTrialBrowserTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppFileHandlingOriginTrialTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
