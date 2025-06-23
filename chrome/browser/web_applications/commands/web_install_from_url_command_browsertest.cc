// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/command_metrics_test_helper.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
constexpr apps::LaunchSource kLaunchSource =
    apps::LaunchSource::kFromWebInstallApi;
constexpr char kAbortError[] = "AbortError";
constexpr char kDataError[] = "DataError";
}  // namespace

namespace web_app {

// Used to test variations of the `WebAppFilter::LaunchableFromInstallApi()`
// where this command is essentially being used to reinstall an app that doesn't
// meet the launch criteria specified via the filter.
enum class NotLaunchableFromInstallApi {
  // By default,
  kNoOSIntegration,
  kDisplayModeBrowser,
};

class WebInstallFromUrlCommandBrowserTest
    : public WebAppBrowserTestBase,
      public ::testing::WithParamInterface<NotLaunchableFromInstallApi> {
 public:
  WebInstallFromUrlCommandBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppInstallation);
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    secondary_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(secondary_server_.Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Tests start on an about:blank page. We need to navigate to any valid URL
  // before we can execute `navigator.install()`
  void NavigateToValidUrl(Browser* app_browser = nullptr) {
    VLOG(0) << https_server()->GetURL("/simple.html").spec();
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(app_browser ? app_browser : browser(),
                                     https_server()->GetURL("/simple.html")));
  }

  // When the permission prompt shows, it must be granted or denied.
  void SetPermissionResponse(bool permission_granted,
                             content::WebContents* contents = nullptr) {
    permissions::PermissionRequestManager::AutoResponseType response =
        permission_granted
            ? permissions::PermissionRequestManager::AutoResponseType::
                  ACCEPT_ALL
            : permissions::PermissionRequestManager::AutoResponseType::DENY_ALL;

    permissions::PermissionRequestManager::FromWebContents(
        contents ? contents : web_contents())
        ->set_auto_response_for_test(response);
  }

  // 2 param navigator.install(install_url, manifest_id)
  bool TryInstallApp(std::string install_url,
                     std::string manifest_id,
                     content::WebContents* contents = nullptr) {
    std::string script = "navigator.install('" + install_url + "', '" +
                         manifest_id +
                         "').then(result => {"
                         "  webInstallResult = result;"
                         "}).catch(error => {"
                         "  webInstallError = error;"
                         "});";
    return ExecJs(contents ? contents : web_contents(), script);
  }

  // 1 param navigator.install(install_url)
  bool TryInstallApp(std::string install_url,
                     content::WebContents* contents = nullptr) {
    std::string script = "navigator.install('" + install_url +
                         "').then(result => {"
                         "  webInstallResult = result;"
                         "}).catch(error => {"
                         "  webInstallError = error;"
                         "});";

    return ExecJs(contents ? contents : web_contents(), script);
  }

  bool ResultExists(content::WebContents* contents = nullptr) {
    // ExecJs returns false when an error is thrown, including when a variable
    // is undefined.
    return ExecJs(contents ? contents : web_contents(), "webInstallResult");
  }

  bool ErrorExists(content::WebContents* contents = nullptr) {
    // ExecJs returns false when an error is thrown, including when a variable
    // is undefined.
    return ExecJs(contents ? contents : web_contents(), "webInstallError");
  }

  std::string GetManifestIdResult(content::WebContents* contents = nullptr) {
    return EvalJs(contents ? contents : web_contents(),
                  "webInstallResult.manifestId")
        .ExtractString();
  }

  std::string GetErrorName() {
    return EvalJs(web_contents(), "webInstallError.name").ExtractString();
  }

 protected:
  net::EmbeddedTestServer secondary_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

///////////////////////////////////////////////////////////////////////////////
// Intended use cases -- 1 and 2 parameter -- for sites that meet
// all manifest id requirements. We expect successful installs here.
///////////////////////////////////////////////////////////////////////////////
IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_OneParam) {
  NavigateToValidUrl();

  // Requires an `install_url` of a document with an `id` field in its
  // manifest.json.
  std::string install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html").spec();

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallApp(install_url));

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_TwoParam) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  base::HistogramTester histograms;
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_FromPWAWindow) {
  // Install setup
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  ui_test_utils::BrowserChangeObserver wait_for_web_app(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  base::HistogramTester histograms;

  // Install the pwa to use to call `navigator.install()` from within.
  webapps::AppId app_id = InstallWebAppFromPage(
      browser(), https_server()->GetURL("/banners/manifest_test_page.html"));
  Browser* app_browser = wait_for_web_app.Wait();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);

  // app to install with `navigator.install()`.
  const GURL install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", install_url).spec();

  base::AutoReset<bool> auto_accept =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  // NavigateToValidUrl(app_browser);
  SetPermissionResponse(/*permission_granted=*/true, app_web_contents);
  // !Important! Because the 2 apps share a scope, we need to pass manifest_id
  // here to ensure an accurate app lookup. If we don't, we'll end up matching
  // the app installed first and launching it. See web_install_service_impl.cc
  // `IsAppInstalled` for more details.
  ASSERT_TRUE(TryInstallApp(install_url.spec(), manifest_id, app_web_contents));

  EXPECT_TRUE(ResultExists(app_web_contents));
  EXPECT_FALSE(ErrorExists(app_web_contents));
  EXPECT_EQ(GetManifestIdResult(app_web_contents), manifest_id);

  // Another app should've launched.
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);
}

///////////////////////////////////////////////////////////////////////////////
// Permissions handling
///////////////////////////////////////////////////////////////////////////////
IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_SameOrigin_AllowPermission) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_TRUE(ResultExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id);
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_SameOrigin_DenyPermission) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  SetPermissionResponse(/*permission_granted=*/false);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_CrossOrigin_AllowPermission) {
  // Navigate to a valid URL on the primary server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/simple.html")));

  std::string install_url =
      secondary_server_
          .GetURL("/banners/manifest_test_page.html?manifest=manifest.json")
          .spec();
  std::string manifest_id =
      secondary_server_.GetURL("/banners/manifest_test_page.html").spec();
  base::HistogramTester histograms;

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_TRUE(ResultExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id);
  EXPECT_FALSE(ErrorExists());

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       InstallApp_CrossOrigin_DenyPermission) {
  // Navigate to a valid URL on the primary server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/simple.html")));

  std::string install_url =
      secondary_server_
          .GetURL("/banners/manifest_test_page.html?manifest=manifest.json")
          .spec();
  std::string manifest_id =
      secondary_server_.GetURL("/banners/manifest_test_page.html").spec();
  SetPermissionResponse(/*permission_granted=*/false);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

// Collection of tests for calling `navigator.install(already_installed_url)`.
// In these cases we show the `WebAppLaunchDialog` to allow the user to launch
// or not.
using WebInstallBackgroundAppAlreadyInstalledBrowserTest =
    WebInstallFromUrlCommandBrowserTest;

IN_PROC_BROWSER_TEST_F(WebInstallBackgroundAppAlreadyInstalledBrowserTest,
                       UserAcceptsLaunchDialog) {
  NavigateToValidUrl();
  base::HistogramTester histograms;

  // Install a background document.
  const GURL background_doc_install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", background_doc_install_url).spec();

  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), background_doc_install_url);
  // Verify that the app was installed and launched.
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);

  // Initiate another install request for the same background document.
  base::AutoReset<bool> auto_accept =
      SetAutoAcceptWebInstallLaunchDialogForTesting();
  // Because we didn't install via web install, we'll be prompted to allow
  // permission before the launch.
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(background_doc_install_url.spec()));
  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id);
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromWebInstallApi, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallBackgroundAppAlreadyInstalledBrowserTest,
                       UserAcceptsLaunchDialog_WithManifestId) {
  NavigateToValidUrl();
  base::HistogramTester histograms;

  // Install a background document.
  const GURL background_doc_install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", background_doc_install_url).spec();

  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), background_doc_install_url);
  // Verify that the app was installed and launched.
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);

  // Initiate another install request for the same background document.
  base::AutoReset<bool> auto_accept =
      SetAutoAcceptWebInstallLaunchDialogForTesting();
  // Because we didn't install via web install, we'll be prompted to allow
  // permission before the launch.
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(background_doc_install_url.spec(), manifest_id));
  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id);
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromWebInstallApi, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallBackgroundAppAlreadyInstalledBrowserTest,
                       UserCancelsLaunchDialog) {
  NavigateToValidUrl();
  base::HistogramTester histograms;

  // Install a background document.
  const GURL background_doc_install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", background_doc_install_url).spec();

  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), background_doc_install_url);
  // Verify that the app was installed and launched.
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);

  // Because we didn't install via web install, we'll be prompted to allow
  // permission before the launch.
  SetPermissionResponse(/*permission_granted=*/true);
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebInstallLaunchDialog");

  // Trigger the launch dialog by initiating another install request for the
  // same background document.
  ExecuteScriptAsync(web_contents(), "navigator.install('" +
                                         background_doc_install_url.spec() +
                                         "').then(result => {"
                                         "  webInstallResult = result;"
                                         "}).catch(error => {"
                                         "  webInstallError = error;"
                                         "});");

  // Wait for the launch dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);
  // Simulate the user clicking the cancel button.
  views::test::CancelDialog(widget);
  destroyed.Wait();

  // Even though the app is installed, because the user did not accept the
  // launch dialog, we should not have a result to prevent fingerprinting
  // concerns.
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromWebInstallApi, 0);
}

IN_PROC_BROWSER_TEST_F(WebInstallBackgroundAppAlreadyInstalledBrowserTest,
                       LaunchDialogClosesOnTabSwitch) {
  NavigateToValidUrl();
  base::HistogramTester histograms;

  // Install a background document.
  const GURL background_doc_install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", background_doc_install_url).spec();

  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), background_doc_install_url);
  // Verify that the app was installed and launched.
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);

  // Because we didn't install via web install, we'll be prompted to allow
  // permission before the launch.
  SetPermissionResponse(/*permission_granted=*/true);
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebInstallLaunchDialog");

  // Trigger the launch dialog by initiating another install request for the
  // same background document.
  ExecuteScriptAsync(web_contents(), "navigator.install('" +
                                         background_doc_install_url.spec() +
                                         "').then(result => {"
                                         "  webInstallResult = result;"
                                         "}).catch(error => {"
                                         "  webInstallError = error;"
                                         "});");

  // Wait for the launch dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  // Switch to a different tab, which should dismiss the dialog.
  chrome::NewTab(browser());

  destroyed.Wait();

  // Switch back to the tab with the app to validate JS results.
  chrome::SelectPreviousTab(browser());
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromWebInstallApi, 0);
}

IN_PROC_BROWSER_TEST_F(WebInstallBackgroundAppAlreadyInstalledBrowserTest,
                       UserAcceptsLaunchDialogWithinPWAWindow) {
  NavigateToValidUrl();
  base::HistogramTester histograms;

  // Prepare to install an app.
  const GURL install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", install_url).spec();
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  ui_test_utils::BrowserChangeObserver wait_for_web_app(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  webapps::AppId app_id =
      web_app::InstallWebAppFromPage(browser(), install_url);
  Browser* app_browser = wait_for_web_app.Wait();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromReparenting, 1);

  // Initiate another install request for the same background document.
  base::AutoReset<bool> auto_accept =
      SetAutoAcceptWebInstallLaunchDialogForTesting();
  // Because we didn't install via web install, we'll be prompted to allow
  // permission before the launch.
  SetPermissionResponse(/*permission_granted=*/true, app_web_contents);

  // Navigate the PWA window to a valid URL and initiate the install.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      app_browser, https_server()->GetURL("/simple.html")));
  ASSERT_TRUE(TryInstallApp(install_url.spec(), app_web_contents));
  EXPECT_TRUE(ResultExists(app_web_contents));
  EXPECT_FALSE(ErrorExists(app_web_contents));
  EXPECT_EQ(GetManifestIdResult(app_web_contents), manifest_id);
  histograms.ExpectBucketCount("WebApp.LaunchSource",
                               apps::LaunchSource::kFromWebInstallApi, 1);
}

// Parameterized test for calling `navigator.install()` on an already
// installed app that does *not satisfy our launch requirements*. In these
// cases we expect the web app *install* dialog is shown. If the user accepts,
// then WebInstallFromUrlCommand will essentially reinstall the app with OS
// integration and launch it in a standalone window.
IN_PROC_BROWSER_TEST_P(WebInstallFromUrlCommandBrowserTest, LaunchApp) {
  // Validates that calling `navigator.install()` on an already installed app
  // that does not satisfy our launch requirements will essentially reinstall
  // the app as a fully OS integrated, standalone-windowed app.
  const GURL install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const GURL manifest_url =
      https_server()->GetURL("/banners/manifest_with_id.json");
  const GURL manifest_id = GenerateManifestId("some_id", install_url);

  auto info_result =
      WebAppInstallInfo::Create(manifest_url, manifest_id, install_url);
  ASSERT_TRUE(info_result.has_value());
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>(std::move(info_result.value()));

  webapps::AppId app_id;
  // Install a variety of apps that don't meet the launch requirements.
  switch (GetParam()) {
    case NotLaunchableFromInstallApi::kNoOSIntegration:
      app_id = test::InstallWebAppWithoutOsIntegration(
          profile(), std::move(info),
          /*overwrite_existing_manifest_fields=*/false,
          webapps::WebappInstallSource::EXTERNAL_DEFAULT);
      break;
    case NotLaunchableFromInstallApi::kDisplayModeBrowser:
      // Simulate the user unchecking "Open in window" in chrome://apps.
      info->user_display_mode = mojom::UserDisplayMode::kBrowser;

      app_id =
          test::InstallWebApp(profile(), std::move(info),
                              /*overwrite_existing_manifest_fields=*/false,
                              webapps::WebappInstallSource::EXTERNAL_DEFAULT);
      break;
  }

  // Check the app's OS integration status
  web_app::WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  auto& registrar = provider->registrar_unsafe();
  ASSERT_TRUE(provider);
  switch (GetParam()) {
    case NotLaunchableFromInstallApi::kNoOSIntegration:
      EXPECT_NE(registrar.GetAppById(app_id)->install_state(),
                proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
      break;
    case NotLaunchableFromInstallApi::kDisplayModeBrowser:
      EXPECT_EQ(registrar.GetAppById(app_id)->install_state(),
                proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
      break;
  }

  // Prepare to invoke navigator.install for the already installed app, which
  // should initiate the *install* dialog.
  base::HistogramTester histograms;
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  SetPermissionResponse(/*permission_granted=*/true);

  NavigateToValidUrl();
  ASSERT_TRUE(TryInstallApp(install_url.spec()));

  EXPECT_TRUE(ResultExists());
  EXPECT_FALSE(ErrorExists());
  EXPECT_EQ(GetManifestIdResult(), manifest_id.spec());

  // Verify the app was reinstalled.
  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  histograms.ExpectUniqueSample("WebApp.LaunchSource", kLaunchSource, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  // It should always have OS integration and launch in an app window.
  EXPECT_EQ(registrar.GetAppById(app_id)->install_state(),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  // The app we're installing specifies display mode as `kFullscreen`, which is
  // a type of standalone window.
  EXPECT_EQ(registrar.GetAppById(app_id)->display_mode(),
            blink::mojom::DisplayMode::kFullscreen);

  // It should also indicate that it was installed via the web install API.
  EXPECT_EQ(registrar.GetAppById(app_id)->latest_install_source(),
            kInstallSource);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebInstallFromUrlCommandBrowserTest,
    testing::Values(NotLaunchableFromInstallApi::kNoOSIntegration,
                    NotLaunchableFromInstallApi::kDisplayModeBrowser));

///////////////////////////////////////////////////////////////////////////////
// Error cases - bad manifests, invalid URLs, etc
///////////////////////////////////////////////////////////////////////////////
IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, NoManifest) {
  NavigateToValidUrl();

  // If the site does not have a manifest, the manifest_id will default to the
  std::string install_url =
      https_server()->GetURL("/banners/no_manifest_test_page.html").spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(
      histograms,
      test::ForAllGetAllSamples(
          test::GetInstallCommandResultHistogramNames(".WebInstallFromUrl",
                                                      ".Crafted"),
          base::BucketsAre(base::Bucket(
              webapps::InstallResultCode::kNotValidManifestForWebApp, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InvalidManifest) {
  NavigateToValidUrl();

  // If the site has an invalid manifest, the manifest_id defaults to the
  std::string install_url =
      https_server()->GetURL("/banners/invalid_manifest_test_page.html").spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(
      histograms,
      test::ForAllGetAllSamples(
          test::GetInstallCommandResultHistogramNames(".WebInstallFromUrl",
                                                      ".Crafted"),
          base::BucketsAre(base::Bucket(
              webapps::InstallResultCode::kNotValidManifestForWebApp, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       ManifestIdMismatch) {
  NavigateToValidUrl();

  // The computed manifest id of this app is the same as the install_url.
  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = https_server()->GetURL("/incorrect_id").spec();
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kDataError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kManifestIdMismatch, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, ManifestMissingId) {
  NavigateToValidUrl();

  // No id specified in the manifest.json
  std::string install_url = GetInstallableAppURL().spec();
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);

  ASSERT_TRUE(TryInstallApp(install_url));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kDataError);

  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kNoCustomManifestId, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest,
                       ManifestWithNoIcons) {
  NavigateToValidUrl();

  // The computed manifest id of this app is the same as the install_url.
  std::string install_url =
      GetAppURLWithManifest("/banners/manifest_no_icon.json").spec();
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(
      histograms,
      test::ForAllGetAllSamples(
          test::GetInstallCommandResultHistogramNames(".WebInstallFromUrl",
                                                      ".Crafted"),
          base::BucketsAre(base::Bucket(
              webapps::InstallResultCode::kNotValidManifestForWebApp, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandBrowserTest, InvalidInstallUrl) {
  NavigateToValidUrl();

  // If the site does not have a manifest, the manifest_id will default to the
  // current document.
  std::string install_url = "https://invalid.url";
  std::string manifest_id = install_url;
  base::HistogramTester histograms;
  SetPermissionResponse(/*permission_granted=*/true);
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);

  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kInstallURLLoadFailed, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".WebInstallFromUrl", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

class WebInstallFromUrlCommandDialogTest
    : public WebInstallFromUrlCommandBrowserTest {
 public:
  SkBitmap ReadImageFile(const base::FilePath& file_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::optional<std::vector<uint8_t>> file_contents =
        base::ReadFileToBytes(file_path);

    return gfx::PNGCodec::Decode(file_contents.value());
  }

  std::u16string GetAppTitle() {
    return u"Manifest test app with id specified";
  }

  base::FilePath GetIconPath() {
    base::FilePath path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &path);
    return path.AppendASCII("banners").AppendASCII("launcher-icon-1x.png");
  }
};

IN_PROC_BROWSER_TEST_F(WebInstallFromUrlCommandDialogTest,
                       VerifyInstallDialogContents) {
  // Go to /simple.html
  NavigateToValidUrl();

  // Target a different page to install.
  const GURL install_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");

  SetPermissionResponse(/*permission_granted=*/true);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppSimpleInstallDialog");

  // We don't actually care about the result of the install, and EvalJs blocks
  // until the promise resolves, which only happens after the dialog is
  // closed. Execute the install asynchronously so we can actually check the
  // dialog contents without the promise timing out.
  ExecuteScriptAsync(web_contents(),
                     "navigator.install('" + install_url.spec() + "');");

  // Wait for the install dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  views::ElementTrackerViews* tracker_views =
      views::ElementTrackerViews::GetInstance();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(widget);

  // Get the icon from the dialog.
  views::ImageView* icon_view =
      tracker_views->GetUniqueViewAs<views::ImageView>(
          kSimpleInstallDialogIconView, context);
  ASSERT_NE(icon_view, nullptr);

  // Convert to a bitmap.
  const ui::ImageModel& icon_view_model = icon_view->GetImageModel();
  ASSERT_FALSE(icon_view_model.IsEmpty());
  ASSERT_TRUE(icon_view_model.IsImage());
  const SkBitmap* dialog_icon_bitmap = icon_view_model.GetImage().ToSkBitmap();
  CHECK(!dialog_icon_bitmap->isNull());

  // Read the expected bitmap from the test data directory.
  base::FilePath path = GetIconPath();
  SkBitmap bitmap_from_png = ReadImageFile(path);
  CHECK(!bitmap_from_png.isNull());
  // The dialog resizes the icon. Resize the png to match.
  bitmap_from_png = skia::ImageOperations::Resize(
      bitmap_from_png, skia::ImageOperations::RESIZE_BEST,
      dialog_icon_bitmap->width(), dialog_icon_bitmap->height());

  EXPECT_TRUE(
      gfx::test::AreBitmapsClose(*dialog_icon_bitmap, bitmap_from_png, 3));

  // Verify the app title label.
  views::Label* app_title_view = tracker_views->GetUniqueViewAs<views::Label>(
      kSimpleInstallDialogAppTitle, context);
  ASSERT_NE(app_title_view, nullptr);
  EXPECT_EQ(app_title_view->GetText(), GetAppTitle());

  // Verify the origin label.
  views::Label* start_url_view = tracker_views->GetUniqueViewAs<views::Label>(
      kSimpleInstallDialogOriginLabel, context);
  ASSERT_NE(start_url_view, nullptr);
  EXPECT_EQ(
      start_url_view->GetText(),
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          install_url));
}
}  // namespace web_app
