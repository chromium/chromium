// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/command_metrics_test_helper.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_install_service_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
constexpr char kAbortError[] = "AbortError";
constexpr char kDataError[] = "DataError";
constexpr char kNotAllowedError[] = "NotAllowedError";
constexpr char kTypeError[] = "TypeError";
}  // namespace

enum class APISignature {
  kZeroParameter,
  kOneParameter,
  kTwoParameter,
};

namespace web_app {
class WebInstallCurrentDocumentBrowserTest
    : public WebAppBrowserTestBase,
      public ::testing::WithParamInterface<APISignature> {
 public:
  WebInstallCurrentDocumentBrowserTest() = default;

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    webapps::TestAppBannerManagerDesktop::SetUp();
  }

  // 0 parameter navigator.install()
  bool TryInstallApp() {
    const std::string script =
        "navigator.install()"
        ".then(result => {"
        "  webInstallResult = result;"
        "}).catch(error => {"
        "  webInstallError = error;"
        "});";

    return ExecJs(web_contents(), script);
  }

  // 1 param navigator.install(install_url)
  bool TryInstallApp(std::string install_url) {
    const std::string script = "navigator.install('" + install_url +
                               "').then(result => {"
                               "  webInstallResult = result;"
                               "}).catch(error => {"
                               "  webInstallError = error;"
                               "});";

    return ExecJs(web_contents(), script);
  }

  // 2 param navigator.install(install_url, manifest_id)
  // `with_gesture` behavior handling is identical for all 3 signatures, so
  // only test with the 2 param signature to avoid redundancy.
  bool TryInstallApp(std::string install_url,
                     std::string manifest_id,
                     bool with_gesture = true) {
    const std::string script = "navigator.install('" + install_url + "', '" +
                               manifest_id +
                               "').then(result => {"
                               "  webInstallResult = result;"
                               "}).catch(error => {"
                               "  webInstallError = error;"
                               "});";

    if (with_gesture) {
      return ExecJs(web_contents(), script);
    } else {
      return ExecJs(web_contents(), script,
                    content::EXECUTE_SCRIPT_NO_USER_GESTURE);
    }
  }

  // Tests start on an about:blank page. We need to navigate to any valid URL
  // before we can execute `navigator.install()`
  void NavigateToValidUrl() {
    VLOG(0) << https_server()->GetURL("/simple.html").spec();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server()->GetURL("/simple.html")));
  }

  void NavigateAndConfigureCurrentDocumentForInstall(
      const GURL& current_doc_url) {
    auto* manager =
        webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), current_doc_url));
    manager->WaitForInstallableCheck();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool ResultExists(content::WebContents* contents = nullptr) {
    if (!contents) {
      contents = web_contents();
    }
    // ExecJs returns false when an error is thrown, including when a variable
    // is undefined.
    return ExecJs(contents, "webInstallResult");
  }

  bool ErrorExists(content::WebContents* contents = nullptr) {
    if (!contents) {
      contents = web_contents();
    }
    // ExecJs returns false when an error is thrown, including when a variable
    // is undefined.
    return ExecJs(contents, "webInstallError");
  }

  const std::string GetManifestIdResult(
      content::WebContents* contents = nullptr) {
    if (!contents) {
      contents = web_contents();
    }
    return EvalJs(contents, "webInstallResult.manifestId").ExtractString();
  }

  const std::string GetErrorName(content::WebContents* contents = nullptr) {
    if (!contents) {
      contents = web_contents();
    }
    return EvalJs(contents, "webInstallError.name").ExtractString();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppInstallation};
};

IN_PROC_BROWSER_TEST_P(WebInstallCurrentDocumentBrowserTest, Install) {
  GURL current_doc_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", current_doc_url).spec();

  NavigateAndConfigureCurrentDocumentForInstall(current_doc_url);
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  SetInstalledCallbackForTesting(install_future.GetCallback());
  base::HistogramTester histograms;

  ui_test_utils::BrowserChangeObserver wait_for_web_app(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  switch (GetParam()) {
    case APISignature::kZeroParameter:
      ASSERT_TRUE(TryInstallApp());
      break;
    case APISignature::kOneParameter:
      ASSERT_TRUE(TryInstallApp(current_doc_url.spec()));
      break;
    case APISignature::kTwoParameter:
      ASSERT_TRUE(TryInstallApp(current_doc_url.spec(), manifest_id));
      break;
  }

  // Verify that the app was installed.
  EXPECT_TRUE(install_future.Wait());
  ASSERT_TRUE(install_future.Get<webapps::InstallResultCode>() ==
              webapps::InstallResultCode::kSuccessNewInstall);

  // Verify that the app was launched.
  auto* app_browser = wait_for_web_app.Wait();
  ASSERT_TRUE(web_app::AppBrowserController::IsWebApp(app_browser));
  auto* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Validate JS results.
  EXPECT_TRUE(ResultExists(app_web_contents));
  EXPECT_FALSE(ErrorExists(app_web_contents));

  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  // Current document installs launch via reparenting.
  histograms.ExpectUniqueSample("WebApp.LaunchSource",
                                apps::LaunchSource::kFromReparenting, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  // TODO(crbug.com/402806158): Log the correct InstallMetrics for current
  // document installs. Until we refactor all the commands, just verify that
  // FetchManifestAndInstall was logged, as that's what current doc installs
  // are using for now.
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandResultHistogramNames(
                      ".FetchManifestAndInstall", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::InstallResultCode::kSuccessNewInstall, 1))));
  EXPECT_THAT(histograms,
              test::ForAllGetAllSamples(
                  test::GetInstallCommandSourceHistogramNames(
                      ".FetchManifestAndInstall", ".Crafted"),
                  base::BucketsAre(base::Bucket(
                      webapps::WebappInstallSource::WEB_INSTALL, 1))));
}

IN_PROC_BROWSER_TEST_P(WebInstallCurrentDocumentBrowserTest,
                       UserAcceptsOpenDialog) {
  GURL current_doc_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", current_doc_url).spec();

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  base::HistogramTester histograms;

  // Install current doc, wait for app browser window to appear and close it.
  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), current_doc_url);
  // Verify that the app was installed and launched.
  histograms.ExpectUniqueSample("WebApp.LaunchSource",
                                apps::LaunchSource::kFromReparenting, 1);

  // Navigate again to the just installed current doc in the browser window.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), current_doc_url));
  auto auto_accept_intent_picker =
      IntentPickerBubbleView::SetAutoAcceptIntentPickerBubbleForTesting();

  ui_test_utils::BrowserChangeObserver wait_for_launch_app(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  // Call navigator.install() to trigger the intent picker.
  switch (GetParam()) {
    case APISignature::kZeroParameter:
      ASSERT_TRUE(TryInstallApp());
      break;
    case APISignature::kOneParameter:
      ASSERT_TRUE(TryInstallApp(current_doc_url.spec()));
      break;
    case APISignature::kTwoParameter:
      ASSERT_TRUE(TryInstallApp(current_doc_url.spec(), manifest_id));
      break;
  }

  // Verify the app was launched again after accepting the intent picker.
  auto* launched_app_browser = wait_for_launch_app.Wait();
  ASSERT_TRUE(web_app::AppBrowserController::IsWebApp(launched_app_browser));
  auto* launched_app_web_contents =
      launched_app_browser->tab_strip_model()->GetActiveWebContents();

  // Validate JS results.
  EXPECT_TRUE(ResultExists(launched_app_web_contents));
  EXPECT_FALSE(ErrorExists(launched_app_web_contents));

  histograms.ExpectUniqueSample("WebApp.LaunchSource",
                                apps::LaunchSource::kFromReparenting, 2);
}

IN_PROC_BROWSER_TEST_P(WebInstallCurrentDocumentBrowserTest,
                       UserCancelsOpenDialog) {
  GURL current_doc_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      GenerateManifestId("some_id", current_doc_url).spec();

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  base::HistogramTester histograms;

  // Install current doc, wait for app browser window to appear and close it.
  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), current_doc_url);
  // Verify that the app was installed and launched.
  histograms.ExpectUniqueSample("WebApp.LaunchSource",
                                apps::LaunchSource::kFromReparenting, 1);

  // Navigate again to the just installed current doc in the browser window.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), current_doc_url));
  auto auto_cancel_intent_picker =
      IntentPickerBubbleView::SetAutoCancelIntentPickerBubbleForTesting();

  // Call navigator.install() to trigger the intent picker.
  switch (GetParam()) {
    case APISignature::kZeroParameter:
      ASSERT_TRUE(TryInstallApp());
      break;
    case APISignature::kOneParameter:
      ASSERT_TRUE(TryInstallApp(current_doc_url.spec()));
      break;
    case APISignature::kTwoParameter:
      ASSERT_TRUE(TryInstallApp(current_doc_url.spec(), manifest_id));
      break;
  }

  // Validate JS results.
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
}

INSTANTIATE_TEST_SUITE_P(,
                         WebInstallCurrentDocumentBrowserTest,
                         testing::Values(APISignature::kZeroParameter,
                                         APISignature::kOneParameter,
                                         APISignature::kTwoParameter));

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTest,
                       IntentPickerAfterTabSwitching) {
  GURL current_doc_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");

  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();
  base::HistogramTester histograms;

  // Install current doc, wait for app browser window to appear and close it.
  webapps::AppId app_id = web_app::InstallWebAppFromPageAndCloseAppBrowser(
      browser(), current_doc_url);
  // Verify that the app was installed and launched.
  histograms.ExpectUniqueSample("WebApp.LaunchSource",
                                apps::LaunchSource::kFromReparenting, 1);

  views::NamedWidgetShownWaiter intent_picker_bubble_shown(
      views::test::AnyWidgetTestPasskey{},
      IntentPickerBubbleView::kViewClassName);

  // Navigate again to the just installed current doc in the browser window.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), current_doc_url));

  // EvalJs blocks until the promise resolves which only happens after the
  // dialog is closed. Execute the script asynchronously so we can change tabs
  // before the promise times out.
  ExecuteScriptAsync(web_contents(),
                     "navigator.install()"
                     ".then(result => {"
                     "  webInstallResult = result;"
                     "}).catch(error => {"
                     "  webInstallError = error;"
                     "});");

  // Wait for the intent picker bubble to show.
  views::Widget* intent_picker =
      intent_picker_bubble_shown.WaitIfNeededAndGet();
  ASSERT_NE(intent_picker, nullptr);

  // Change focus to a new tab.
  chrome::NewTab(browser());

  // Switch back to the tab with the app to validate JS results.
  chrome::SelectPreviousTab(browser());
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

// Manifest validation for current document installs.
using WebInstallCurrentDocumentBrowserTestManifestErrors =
    WebInstallCurrentDocumentBrowserTest;

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTestManifestErrors,
                       NoManifest) {
  GURL current_doc_url =
      https_server()->GetURL("/banners/no_manifest_test_page.html");

  NavigateAndConfigureCurrentDocumentForInstall(current_doc_url);

  ASSERT_TRUE(TryInstallApp(current_doc_url.spec()));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTestManifestErrors,
                       MissingId) {
  GURL current_doc_url = GetInstallableAppURL();

  NavigateAndConfigureCurrentDocumentForInstall(current_doc_url);

  ASSERT_TRUE(TryInstallApp(current_doc_url.spec()));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kDataError);
}

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTestManifestErrors,
                       IdMismatch) {
  // Has "id": "some_id"
  GURL current_doc_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");
  const std::string manifest_id =
      https_server()->GetURL("/incorrect_id").spec();

  NavigateAndConfigureCurrentDocumentForInstall(current_doc_url);

  ASSERT_TRUE(TryInstallApp(current_doc_url.spec(), manifest_id));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kDataError);
}

// Implementation-generic tests for bad JavaScript API inputs.
using WebInstallServiceImplBrowserTestBadInput =
    WebInstallCurrentDocumentBrowserTest;

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       MissingUserGesture) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  ASSERT_TRUE(TryInstallApp(install_url, manifest_id, /*with_gesture=*/false));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kNotAllowedError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       OneParam_Undefined) {
  NavigateToValidUrl();

  const std::string script =
      "let install_url;"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       OneParam_Null) {
  NavigateToValidUrl();

  const std::string script =
      "let install_url=null;"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       OneParam_Number) {
  NavigateToValidUrl();

  const std::string script =
      "let install_url = new Number(1);"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       OneParam_Empty) {
  NavigateToValidUrl();

  const std::string script =
      "let install_url='';"
      "navigator.install(install_url).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       TwoParams_UndefinedInstallUrl) {
  NavigateToValidUrl();

  const std::string manifest_id = GetInstallableAppURL().spec();
  const std::string script =
      "let install_url;"
      "navigator.install(install_url, '" +
      manifest_id +
      "').then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       TwoParams_UndefinedManifestId) {
  NavigateToValidUrl();

  const std::string install_url = GetInstallableAppURL().spec();
  const std::string script =
      "let manifest_id;"
      "navigator.install('" +
      install_url +
      "', manifest_id).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       TwoParams_EmptyManifestId) {
  NavigateToValidUrl();

  const std::string install_url = GetInstallableAppURL().spec();
  const std::string script =
      "let manifest_id = '';"
      "navigator.install('" +
      install_url +
      "', manifest_id).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       TwoParams_NullManifestId) {
  NavigateToValidUrl();

  const std::string install_url = GetInstallableAppURL().spec();
  const std::string script =
      "let manifest_id = null;"
      "navigator.install('" +
      install_url +
      "', manifest_id).then(result => {"
      "  webInstallResult = result;"
      "}).catch(error => {"
      "  webInstallError = error;"
      "});";
  ASSERT_TRUE(ExecJs(web_contents(), script));

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kTypeError);
}

}  // namespace web_app
