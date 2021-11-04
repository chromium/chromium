// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/file_handling_permission_request_dialog_test_api.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_launch/file_handling_expiry.mojom-test-utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#endif

namespace web_app {

// A fake file handling expiry service. This service allows us to mock having an
// origin trial which expires having a certain time, without needing to manage
// actual origin trial tokens.
class FakeFileHandlingExpiryService
    : public blink::mojom::FileHandlingExpiryInterceptorForTesting {
 public:
  FakeFileHandlingExpiryService()
      : expiry_time_(base::Time::Now() + base::Days(1)) {}

  blink::mojom::FileHandlingExpiry* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
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

class WebAppFileHandlingTestBase : public WebAppControllerBrowserTest {
 public:
  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  WebAppFileHandlerManager& file_handler_manager() {
    return provider()
        ->os_integration_manager()
        .file_handler_manager_for_testing();
  }

  WebAppRegistrar& registrar() { return provider()->registrar(); }

  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetTextFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/blank_page.html");
  }

  GURL GetCSVFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/page_with_refs.html");
  }

  GURL GetHTMLFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/page_with_frame.html");
  }

  void InstallFileHandlingPWA() {
    GURL url = GetSecureAppURL();

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->title = u"A Hosted App";

    // Basic plain text format.
    apps::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.accept.emplace_back();
    entry1.accept[0].mime_type = "text/*";
    entry1.accept[0].file_extensions.insert(".txt");
    web_app_info->file_handlers.push_back(std::move(entry1));

    // A format that the browser is also a handler for, to confirm that the
    // browser doesn't override PWAs using File Handling for types that the
    // browser also handles.
    apps::FileHandler entry2;
    entry2.action = GetHTMLFileHandlerActionURL();
    entry2.accept.emplace_back();
    entry2.accept[0].mime_type = "text/html";
    entry2.accept[0].file_extensions.insert(".html");
    web_app_info->file_handlers.push_back(std::move(entry2));

    // application/* format.
    apps::FileHandler entry3;
    entry3.action = GetCSVFileHandlerActionURL();
    entry3.accept.emplace_back();
    entry3.accept[0].mime_type = "application/csv";
    entry3.accept[0].file_extensions.insert(".csv");
    web_app_info->file_handlers.push_back(std::move(entry3));

    app_id_ =
        WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

  void InstallAnotherFileHandlingPwa(const GURL& start_url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"A second app";

    // This one handles jpegs.
    apps::FileHandler entry1;
    entry1.action = GetTextFileHandlerActionURL();
    entry1.accept.emplace_back();
    entry1.accept[0].mime_type = "image/jpeg";
    entry1.accept[0].file_extensions.insert(".jpeg");
    web_app_info->file_handlers.push_back(std::move(entry1));

    WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

 protected:
  ContentSetting GetFileHandlingPermission(const GURL& url) {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    return map->GetContentSetting(url, url, ContentSettingsType::FILE_HANDLING);
  }

  void SetFileHandlingPermission(ContentSetting setting) {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetContentSettingDefaultScope(GetSecureAppURL(), GetSecureAppURL(),
                                       ContentSettingsType::FILE_HANDLING,
                                       setting);
  }

  const AppId& app_id() { return app_id_; }

 private:
  AppId app_id_;
};

namespace {

base::FilePath NewTestFilePath(const base::StringPiece extension) {
  // CreateTemporaryFile blocks, temporarily allow blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // In order to test file handling, we need to be able to supply a file
  // extension for the temp file.
  base::FilePath test_file_path;
  base::CreateTemporaryFile(&test_file_path);
  base::FilePath new_file_path = test_file_path.AddExtensionASCII(extension);
  EXPECT_TRUE(base::ReplaceFile(test_file_path, new_file_path, nullptr));
  return new_file_path;
}

// Launches the |app_id| web app with |files| handles, awaits for
// |expected_launch_url| to load and stashes any launch params on
// "window.launchParams" for further inspection.
content::WebContents* LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    const GURL& expected_launch_url,
    const apps::mojom::LaunchContainer launch_container =
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
    const apps::mojom::LaunchSource launch_source =
        apps::mojom::LaunchSource::kFromTest,
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
          ->LaunchAppWithParams(std::move(params));

  navigation_observer.Wait();

  // Attach the launchParams to the window so we can inspect them easily.
  auto result = content::EvalJs(web_contents,
                                "launchQueue.setConsumer(launchParams => {"
                                "  window.launchParams = launchParams;"
                                "});");

  return web_contents;
}

}  // namespace

enum class FileHandlingGateType {
  kUsesPermission,
  kUsesSetting,
};

class WebAppFileHandlingBrowserTest
    : public WebAppFileHandlingTestBase,
      public testing::WithParamInterface<FileHandlingGateType> {
 public:
  WebAppFileHandlingBrowserTest()
      : WebAppFileHandlingBrowserTest(/*parameterize=*/true) {}

  explicit WebAppFileHandlingBrowserTest(bool parameterize) {
    feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI}, {});
    if (parameterize) {
      feature_list_for_settings_.InitWithFeatureState(
          features::kDesktopPWAsFileHandlingSettingsGated,
          GetParam() == FileHandlingGateType::kUsesSetting);
    }
  }

  bool UsesPermissions() {
    return !base::FeatureList::IsEnabled(
        features::kDesktopPWAsFileHandlingSettingsGated);
  }

  void LaunchWithFiles(
      const std::string& app_id,
      const GURL& expected_launch_url,
      const std::vector<base::FilePath>& files,
      const apps::mojom::LaunchContainer launch_container =
          apps::mojom::LaunchContainer::kLaunchContainerWindow) {
    web_contents_ = LaunchApplication(
        profile(), app_id, expected_launch_url, launch_container,
        apps::mojom::LaunchSource::kFromFileManager, files);
    destroyed_watcher_ =
        std::make_unique<content::WebContentsDestroyedWatcher>(web_contents_);
  }

  void VerifyPwaDidReceiveFileLaunchParams(
      bool expect_got_launch_params,
      const base::FilePath& expected_file_path = {}) {
    bool got_launch_params =
        content::EvalJs(web_contents_, "!!window.launchParams").ExtractBool();
    ASSERT_EQ(expect_got_launch_params, got_launch_params);
    if (got_launch_params) {
      EXPECT_EQ(1, content::EvalJs(web_contents_,
                                   "window.launchParams.files.length"));
      EXPECT_EQ(
          expected_file_path.BaseName().AsUTF8Unsafe(),
          content::EvalJs(web_contents_, "window.launchParams.files[0].name"));
    }
  }

  void UninstallWebApp(const AppId& app_id) {
    base::RunLoop run_loop;
    UninstallWebAppWithCallback(
        profile(), app_id, base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_settings_;
  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<content::WebContentsDestroyedWatcher> destroyed_watcher_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest, ManifestFields) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_app_file_handling/basic_app.html"));
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  ASSERT_EQ(1U, web_app->file_handlers().size());
  EXPECT_EQ(embedded_test_server()->GetURL(
                "/web_app_file_handling/icons_app_load.html"),
            web_app->file_handlers()[0].action);
  EXPECT_EQ(u"Plain Text!", web_app->file_handlers()[0].display_name);
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       LaunchConsumerIsNotTriggeredWithNoFiles) {
  InstallFileHandlingPWA();
  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);
  // The URL used is the normal start URL.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {});
  VerifyPwaDidReceiveFileLaunchParams(false);
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParams) {
  InstallFileHandlingPWA();
  if (UsesPermissions())
    SetFileHandlingPermission(CONTENT_SETTING_ALLOW);
  base::FilePath test_file_path = NewTestFilePath("txt");
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path});

  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path);
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       LaunchConsumerIsNotTriggeredWithPermissionDenied) {
  // TODO(crbug/1245301): update test.
  if (!UsesPermissions())
    return;

  InstallFileHandlingPWA();
  SetFileHandlingPermission(CONTENT_SETTING_BLOCK);
  base::FilePath test_file_path = NewTestFilePath("txt");
  // The URL used is the normal start URL.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {test_file_path});

  VerifyPwaDidReceiveFileLaunchParams(false);
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParamsInTab) {
  InstallFileHandlingPWA();
  if (UsesPermissions())
    SetFileHandlingPermission(CONTENT_SETTING_ALLOW);
  base::FilePath test_file_path = NewTestFilePath("txt");
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);

  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path);
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsDispatchOnCorrectFileHandlingURL) {
  InstallFileHandlingPWA();
  if (UsesPermissions())
    SetFileHandlingPermission(CONTENT_SETTING_ALLOW);

  // Test that file handler dispatches correct URL based on file extension.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {});
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(),
                  {NewTestFilePath("txt")});
  LaunchWithFiles(app_id(), GetHTMLFileHandlerActionURL(),
                  {NewTestFilePath("html")});
  LaunchWithFiles(app_id(), GetCSVFileHandlerActionURL(),
                  {NewTestFilePath("csv")});

  // Test as above in a tab.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(),
                  {NewTestFilePath("txt")},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetHTMLFileHandlerActionURL(),
                  {NewTestFilePath("html")},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
  LaunchWithFiles(app_id(), GetCSVFileHandlerActionURL(),
                  {NewTestFilePath("csv")},
                  apps::mojom::LaunchContainer::kLaunchContainerTab);
}

// Regression test for crbug.com/1205528
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       LaunchParamsEmptyIfFileUnhandled) {
  InstallFileHandlingPWA();
  if (UsesPermissions())
    SetFileHandlingPermission(CONTENT_SETTING_ALLOW);

  // Test that file handler dispatches to the normal start URL when the file
  // path is not a handled file type, and `launchParams` remains undefined.
  LaunchWithFiles(app_id(), GetSecureAppURL(), {NewTestFilePath("png")});
  VerifyPwaDidReceiveFileLaunchParams(false);
}

// Regression test for crbug.com/1126091
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       LaunchQueueSetOnRedirect) {
  // Install an app where the file handling action page redirects.
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url =
      https_server()->GetURL("app.com", "/web_app_file_handling/index.html");
  web_app_info->scope = web_app_info->start_url.GetWithoutFilename();
  web_app_info->title = u"An app that redirects";

  apps::FileHandler entry;
  entry.action = https_server()->GetURL(
      "app.com", "/web_app_file_handling/handle_files_with_redirect.html");
  entry.accept.emplace_back();
  entry.accept[0].mime_type = "text/*";
  entry.accept[0].file_extensions.insert(".txt");
  web_app_info->file_handlers.push_back(std::move(entry));

  AppId app_id =
      WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));

  // Note this must be called after the app is installed, as installing an app
  // with new file handlers resets it.
  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);

  // The redirect points to handle_files.html, so wait till that navigation is
  // finished.
  base::FilePath file = NewTestFilePath("txt");
  LaunchWithFiles(app_id,
                  https_server()->GetURL(
                      "app.com", "/web_app_file_handling/handle_files.html"),
                  {file});

  // The redirected-to page should get the launch queue.
  VerifyPwaDidReceiveFileLaunchParams(true, file);
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       LaunchQueueNotSetOnCrossOriginRedirect) {
  SetFileHandlingPermission(CONTENT_SETTING_ALLOW);
  // Install an app where the file handling action page redirects to a page on a
  // different origin.
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url =
      https_server()->GetURL("app.com", "/web_app_file_handling/index.html");
  web_app_info->scope = web_app_info->start_url.GetWithoutFilename();
  web_app_info->title = u"An app that redirects to a different origin";

  apps::FileHandler entry;
  entry.action = https_server()->GetURL(
      "app.com",
      "/web_app_file_handling/handle_files_with_redirect_to_other_origin.html");
  entry.accept.emplace_back();
  entry.accept[0].mime_type = "text/*";
  entry.accept[0].file_extensions.insert(".txt");
  web_app_info->file_handlers.push_back(std::move(entry));

  AppId app_id =
      WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));

  // The redirect points to handle_files.html with a different origin, so wait
  // till that navigation is finished.
  base::FilePath file = NewTestFilePath("txt");
  LaunchWithFiles(
      app_id,
      https_server()->GetURL("example.com",
                             "/web_app_file_handling/handle_files.html"),
      {file});

  // The redirected-to page should NOT get the launch queue.
  VerifyPwaDidReceiveFileLaunchParams(false);
}

// Tests that when two apps are installed and share an origin (but not scope),
// `GetFileHandlersForAllWebAppsWithOrigin` will report all the file handlers
// across both apps.
IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       FileHandlerAggregationForUi) {
  InstallFileHandlingPWA();
  EXPECT_EQ(3U,
            GetFileHandlersForAllWebAppsWithOrigin(profile(), GetSecureAppURL())
                .size());

  GURL second_app_url = https_server()->GetURL("app.com", "/pwa/app2.html");
  InstallAnotherFileHandlingPwa(second_app_url);
  EXPECT_EQ(2U, registrar().GetAppIds().size());
  EXPECT_EQ(4U,
            GetFileHandlersForAllWebAppsWithOrigin(profile(), GetSecureAppURL())
                .size());
  EXPECT_EQ(
      4U,
      GetFileHandlersForAllWebAppsWithOrigin(profile(), second_app_url).size());

  std::u16string display_string_app1 =
      GetFileTypeAssociationsHandledByWebAppsForDisplay(profile(),
                                                        GetSecureAppURL());
  std::u16string display_string_app2 =
      GetFileTypeAssociationsHandledByWebAppsForDisplay(profile(),
                                                        second_app_url);
  EXPECT_EQ(display_string_app1, display_string_app2);
#if defined(OS_LINUX)
  const std::u16string kHtmlDisplayString = u"text/html";
  const std::u16string kJpegDisplayString = u"image/jpeg";
#else
  const std::u16string kHtmlDisplayString = u"HTML";
  const std::u16string kJpegDisplayString = u"JPEG";
#endif
  EXPECT_NE(std::u16string::npos, display_string_app1.find(kHtmlDisplayString));
  EXPECT_NE(std::u16string::npos, display_string_app1.find(kJpegDisplayString));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       SometimesResetPermission) {
  // Install the first app and simulate the user granting it the file handling
  // permission.
  InstallFileHandlingPWA();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  const GURL origin = GetSecureAppURL().DeprecatedGetOriginAsURL();

  if (UsesPermissions()) {
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(origin, origin,
                                     ContentSettingsType::FILE_HANDLING));
    map->SetContentSettingDefaultScope(origin, origin,
                                       ContentSettingsType::FILE_HANDLING,
                                       CONTENT_SETTING_ALLOW);
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(origin, origin,
                                     ContentSettingsType::FILE_HANDLING));
  } else {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    WebApp* app = update->UpdateApp(app_id());
    ASSERT_TRUE(app);
    EXPECT_EQ(ApiApprovalState::kRequiresPrompt,
              app->file_handler_approval_state());
    app->SetFileHandlerApprovalState(ApiApprovalState::kAllowed);
  }

  // Tangentially: make sure the outparam for
  // `GetFileTypeAssociationsHandledByWebAppsForDisplay` is properly set.
  bool plural = false;
  GetFileTypeAssociationsHandledByWebAppsForDisplay(profile(),
                                                    GetSecureAppURL(), &plural);
  EXPECT_TRUE(plural);

  // Install a second app, which is on the same origin and asks to handle more
  // file types. The permission should have been set back to ASK.
  GURL second_app_url = https_server()->GetURL("app.com", "/pwa/app2.html");
  InstallAnotherFileHandlingPwa(second_app_url);
  if (UsesPermissions()) {
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(origin, origin,
                                     ContentSettingsType::FILE_HANDLING));
  } else {
    // Installing a different app should have no impact when settings are used
    // instead of permissions.
    EXPECT_EQ(ApiApprovalState::kAllowed,
              registrar().GetAppById(app_id())->file_handler_approval_state());

    // The rest of the test is not relevant when using settings.
    return;
  }

  // Set to ALLOW again.
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);

  // Install a third app, which is on a different origin; this should have no
  // effect on the permission.
  GURL third_app_url = https_server()->GetURL("otherapp.com", "/pwa/app2.html");
  InstallAnotherFileHandlingPwa(third_app_url);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  GURL third_app_origin = third_app_url.DeprecatedGetOriginAsURL();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(third_app_origin, third_app_origin,
                                   ContentSettingsType::FILE_HANDLING));
  // Tangentially: make sure the outparam for
  // `GetFileTypeAssociationsHandledByWebAppsForDisplay` is properly set.
  GetFileTypeAssociationsHandledByWebAppsForDisplay(profile(), third_app_url,
                                                    &plural);
  EXPECT_FALSE(plural);

  // Install a fourth app, which is on the same origin but asks for a subset of
  // the file types of the first two. This should have no effect on the
  // permission.
  GURL fourth_app_url = https_server()->GetURL("app.com", "/pwa2/app2.html");
  InstallAnotherFileHandlingPwa(fourth_app_url);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       ResetPermissionOnUninstall) {
  // Install an app and simulate the user granting it the file handling
  // permission.
  InstallFileHandlingPWA();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  const GURL origin = GetSecureAppURL().DeprecatedGetOriginAsURL();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Install a second app, which is on the same origin and does not handle any
  // files. This should not affect the permission.
  GURL second_app_url = https_server()->GetURL("app.com", "/pwa2/app.html");
  InstallPWA(second_app_url);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Uninstall the first app. It should reset the permission since no other app
  // is installed with file handlers.
  UninstallWebApp(app_id());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Install the first app again and grant the permission.
  InstallFileHandlingPWA();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);

  // Install a third app, which is on a different origin; this should have no
  // effect on the permission.
  GURL third_app_url = https_server()->GetURL("otherapp.com", "/pwa/app.html");
  InstallAnotherFileHandlingPwa(third_app_url);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Uninstall the first app. It should reset the permission since no other app
  // is installed *on the same origin* with file handlers.
  UninstallWebApp(app_id());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));

  // Install two PWAs with file handlers on the same origin, then grant the
  // permission. Uninstalling one should not reset the permission.
  InstallFileHandlingPWA();
  InstallAnotherFileHandlingPwa(
      https_server()->GetURL("app.com", "/pwa3/app.html"));
  map->SetContentSettingDefaultScope(origin, origin,
                                     ContentSettingsType::FILE_HANDLING,
                                     CONTENT_SETTING_ALLOW);
  UninstallWebApp(app_id());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(origin, origin,
                                   ContentSettingsType::FILE_HANDLING));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppFileHandlingBrowserTest,
    ::testing::Values(FileHandlingGateType::kUsesPermission,
                      FileHandlingGateType::kUsesSetting));

// TODO(estade): remove this test when kDesktopPWAsFileHandlingSettingsGated is
// removed.
class WebAppFileHandlingPermissionDialogTest
    : public WebAppFileHandlingBrowserTest {
 public:
  WebAppFileHandlingPermissionDialogTest()
      : WebAppFileHandlingBrowserTest(/*parameterize=*/false) {
    feature_list_.InitWithFeatures(
        {}, {features::kDesktopPWAsFileHandlingSettingsGated});
  }

  void InstallAndLaunchWebApp() {
    InstallFileHandlingPWA();
    SetFileHandlingPermission(CONTENT_SETTING_ASK);

    EXPECT_FALSE(FileHandlingPermissionRequestDialogTestApi::IsShowing());

    test_file_path_ = NewTestFilePath("txt");
    LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path_});

    // The permission request is dequeued asynchronously. It may or may not be
    // showing by now.
    if (!FileHandlingPermissionRequestDialogTestApi::IsShowing())
      permissions::PermissionRequestObserver(web_contents_).Wait();

    // A dialog is showing now.
    ASSERT_TRUE(FileHandlingPermissionRequestDialogTestApi::IsShowing());

    // The launch consumer isn't triggered while the dialog is showing.
    VerifyPwaDidReceiveFileLaunchParams(false);
  }

 protected:
  base::FilePath test_file_path_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, AllowAlways) {
  InstallAndLaunchWebApp();
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/true,
                                                      /*accept=*/true);
  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path_);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetFileHandlingPermission(GetSecureAppURL()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, AllowOnce) {
  InstallAndLaunchWebApp();
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/false,
                                                      /*accept=*/true);
  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path_);
  EXPECT_EQ(CONTENT_SETTING_ASK, GetFileHandlingPermission(GetSecureAppURL()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, BlockAlways) {
  InstallAndLaunchWebApp();
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/true,
                                                      /*accept=*/false);
  // Verify that the window is closed.
  destroyed_watcher_->Wait();
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetFileHandlingPermission(GetSecureAppURL()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, BlockOnce) {
  InstallAndLaunchWebApp();
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/false,
                                                      /*accept=*/false);
  // Verify that the window is closed.
  destroyed_watcher_->Wait();
  EXPECT_EQ(CONTENT_SETTING_ASK, GetFileHandlingPermission(GetSecureAppURL()));
}

class WebAppFileHandlingOriginTrialBrowserTest
    : public WebAppFileHandlingTestBase {
 public:
  WebAppFileHandlingOriginTrialBrowserTest() {
    WebAppFileHandlerManager::DisableAutomaticFileHandlerCleanupForTesting();
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

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
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

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
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

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
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
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       PRE_ExpiredTrialHandlersAreCleanedUpAtLaunch) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));

  // Update the expiry time to be in the past.
  UpdateDoubleWebAppPref(profile()->GetPrefs(), app_id(),
                         kFileHandlingOriginTrialExpiryTime,
                         base::Time().ToDoubleT());
}

// Part 2: Test that expired file handlers for an app are cleaned up.
// Disabled due to flakiness. http://crbug.com/1249357
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       DISABLED_ExpiredTrialHandlersAreCleanedUpAtLaunch) {
  EXPECT_EQ(1, file_handler_manager().TriggerFileHandlerCleanupForTesting());
}

// Tests that non expired file handlers are not cleaned up.
// Part 1: Install an app with valid file handlers.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       PRE_ValidFileHandlerAreNotCleanedUpAtLaunch) {
  InstallFileHandlingPWA();
  SetUpInterceptorNavigateToAppAndMaybeWait();
  EXPECT_TRUE(file_handler_manager().AreFileHandlersEnabled(app_id()));
}

// Part 2: Test that expired file handlers for an app are cleaned up.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
                       ValidFileHandlerAreNotCleanedUpAtLaunch) {
  EXPECT_EQ(0, file_handler_manager().TriggerFileHandlerCleanupForTesting());
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
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

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
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

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialBrowserTest,
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

class WebAppFileHandlingOriginTrialTest : public WebAppControllerBrowserTest {
 public:
  explicit WebAppFileHandlingOriginTrialTest(
      std::string origin = "https://file-handling-pwa")
      : origin_(std::move(origin)) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebAppControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialPublicKeyForTesting);
  }

  void TearDownOnMainThread() override { interceptor_.reset(); }

 protected:
  void GrantFileHandlingPermission() {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetDefaultContentSetting(ContentSettingsType::FILE_HANDLING,
                                  CONTENT_SETTING_ALLOW);
  }

  AppId InstallFileHandlingWebApp(GURL* start_url_out = nullptr) {
    // We need to use URLLoaderInterceptor (rather than a EmbeddedTestServer),
    // because origin trial token is associated with a fixed origin, whereas
    // EmbeddedTestServer serves content on a random port.
    interceptor_ =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            kBaseDataDir, GURL(origin_));

    GURL start_url = GURL(origin_ + "/index.html");

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"A Web App";

    apps::FileHandler entry1;
    entry1.action = start_url;
    entry1.accept.emplace_back();
    entry1.accept[0].mime_type = "text/*";
    entry1.accept[0].file_extensions.insert(".txt");
    web_app_info->file_handlers.push_back(std::move(entry1));

    AppId app_id =
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
  const std::string origin_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialTest,
                       LaunchParamsArePassedCorrectly) {
  GURL start_url;
  const AppId app_id = InstallFileHandlingWebApp(&start_url);
  GrantFileHandlingPermission();
  base::FilePath test_file_path = NewTestFilePath("txt");
  content::WebContents* web_content = LaunchApplication(
      profile(), app_id, start_url,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      apps::mojom::LaunchSource::kFromFileManager, {test_file_path});
  EXPECT_EQ(1,
            content::EvalJs(web_content, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_content, "window.launchParams.files[0].name"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       SettingsCategoryVisibility) {
  // The file handling permission is visible in a general context.
  const std::vector<ContentSettingsType>& all_categories =
      site_settings::GetVisiblePermissionCategories();
  EXPECT_FALSE(std::find(all_categories.begin(), all_categories.end(),
                         ContentSettingsType::FILE_HANDLING) ==
               all_categories.end());

  // The file handling permission is not visible in the context of an origin
  // that doesn't correspond to a PWA.
  std::vector<ContentSettingsType> categories_for_arbitrary_website =
      site_settings::GetVisiblePermissionCategoriesForOrigin(
          profile(), GURL("https://example.com"));
  EXPECT_TRUE(std::find(categories_for_arbitrary_website.begin(),
                        categories_for_arbitrary_website.end(),
                        ContentSettingsType::FILE_HANDLING) ==
              categories_for_arbitrary_website.end());

  // The file handling permission *is* visible for a PWA origin.
  InstallFileHandlingPWA();
  std::vector<ContentSettingsType> categories_for_pwa =
      site_settings::GetVisiblePermissionCategoriesForOrigin(
          profile(), GetSecureAppURL().DeprecatedGetOriginAsURL());
  EXPECT_FALSE(std::find(categories_for_pwa.begin(), categories_for_pwa.end(),
                         ContentSettingsType::FILE_HANDLING) ==
               categories_for_pwa.end());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// End-to-end test to ensure the file handler is registered on ChromeOS when the
// extension system is initialized. Gives more coverage than the unit tests.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialTest,
                       IsFileHandlerOnChromeOS) {
  const AppId app_id = InstallFileHandlingWebApp();
  GrantFileHandlingPermission();
  base::FilePath test_file_path = NewTestFilePath("txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);

  // Note that there are normally multiple tasks due to default-installed
  // handlers (e.g. add to zip file). But those handlers are not installed by
  // default in browser tests.
  EXPECT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor.app_id, app_id);
}

class WebAppFileHandlingDisabledTest
    : public WebAppFileHandlingOriginTrialTest {
 public:
  // This origin makes sure the installed web app does not match the origin
  // trial and therefore relies on kFileHandlingAPI being enabled.
  WebAppFileHandlingDisabledTest()
      : WebAppFileHandlingOriginTrialTest(
            "https://file-handling-pwa-no-origin-trial") {
    feature_list_.InitWithFeatures({}, {blink::features::kFileHandlingAPI});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that the web app is not returned as a file handler task when
// the flag kFileHandlingAPI is disabled.
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingDisabledTest,
                       NoFileHandlerOnChromeOS) {
  const AppId app_id = InstallFileHandlingWebApp();
  GrantFileHandlingPermission();
  base::FilePath test_file_path = NewTestFilePath("txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);

  EXPECT_EQ(0u, tasks.size());
}

// Ensures correct behavior for files on "special volumes", such as file systems
// provided by extensions. These do not have local files (i.e. backed by
// inodes).
IN_PROC_BROWSER_TEST_F(WebAppFileHandlingOriginTrialTest,
                       NotHandlerForNonNativeFiles) {
  const AppId app_id = InstallFileHandlingWebApp();
  GrantFileHandlingPermission();
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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WebAppFileHandlingIconBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAppFileHandlingIconBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kFileHandlingAPI,
                                    blink::features::kFileHandlingIcons},
                                   {});
    WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(GetParam());
  }
  ~WebAppFileHandlingIconBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingIconBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_app_file_handling/icons_app.html"));
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  auto* provider = WebAppProvider::GetForTest(browser()->profile());
  const WebApp* web_app = provider->registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  ASSERT_EQ(1U, web_app->file_handlers().size());
  if (WebAppFileHandlerManager::IconsEnabled()) {
    ASSERT_EQ(1U, web_app->file_handlers()[0].downloaded_icons.size());
    EXPECT_EQ(20,
              web_app->file_handlers()[0].downloaded_icons[0].square_size_px);
  } else {
    EXPECT_TRUE(web_app->file_handlers()[0].downloaded_icons.empty());
  }
}

// TODO(crbug.com/1218210): add more tests.

INSTANTIATE_TEST_SUITE_P(, WebAppFileHandlingIconBrowserTest, testing::Bool());

}  // namespace web_app
