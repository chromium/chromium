// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/profiles/profile.h"
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
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "web_install_service_impl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
constexpr char kAbortError[] = "AbortError";
constexpr char kDataError[] = "DataError";
constexpr char kNotAllowedError[] = "NotAllowedError";
constexpr char kTypeError[] = "TypeError";
constexpr char kInstallResultUma[] = "WebApp.WebInstallApi.Result";
constexpr char kInstallTypeUma[] = "WebApp.WebInstallApi.InstallType";
}  // namespace

namespace web_app {
class WebInstallCurrentDocumentBrowserTest : public WebAppBrowserTestBase {
 public:
  WebInstallCurrentDocumentBrowserTest() = default;

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    webapps::TestAppBannerManagerDesktop::SetUp();
  }

  // 0 parameter navigator.install()
  bool TryInstallApp(bool with_gesture = true) {
    const std::string script =
        "navigator.install()"
        ".then(result => {"
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

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTest, Install_NoParams) {
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

  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  ASSERT_TRUE(TryInstallApp());

  // Verify that the app was installed.
  EXPECT_TRUE(install_future.Wait());
  ASSERT_TRUE(install_future.Get<webapps::InstallResultCode>() ==
              webapps::InstallResultCode::kSuccessNewInstall);

  // Verify that the app was launched.
  auto* app_browser = browser_created_observer.Wait();
  ASSERT_TRUE(web_app::AppBrowserController::IsWebApp(app_browser));
  auto* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Validate JS results.
  EXPECT_TRUE(ResultExists(app_web_contents));
  EXPECT_FALSE(ErrorExists(app_web_contents));
  histograms.ExpectBucketCount("Blink.UseCounter.WebDXFeatures",
                               blink::mojom::WebDXFeature::kDRAFT_WebInstallAPI,
                               1);

  // Validate browser results.
  histograms.ExpectUniqueSample("WebApp.Install.Source.Success", kInstallSource,
                                1);
  // Current document installs launch via reparenting.
  histograms.ExpectUniqueSample("WebApp.LaunchSource",
                                apps::LaunchSource::kFromReparenting, 1);
  histograms.ExpectUniqueSample("WebApp.NewCraftedAppInstalled.ByUser",
                                /*sample=*/true, 1);

  histograms.ExpectBucketCount(kInstallResultUma,
                               web_app::WebInstallApiResult::kSuccess, 1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);

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

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTest,
                       UserDeclinesInstallDialog) {
  GURL current_doc_url =
      https_server()->GetURL("/banners/manifest_with_id_test_page.html");

  NavigateAndConfigureCurrentDocumentForInstall(current_doc_url);
  // Simulate the user declining the install dialog.
  auto auto_decline_pwa_install_confirmation =
      SetAutoDeclinePWAInstallConfirmationForTesting();
  base::HistogramTester histograms;

  ASSERT_TRUE(TryInstallApp());

  // Validate JS results.
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);

  // Validate browser results.
  histograms.ExpectUniqueSample("WebApp.Install.Source.Failure", kInstallSource,
                                1);
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kCanceledByUser, 1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTest,
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

  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  // Call navigator.install() to trigger the intent picker.
  ASSERT_TRUE(TryInstallApp());

  // Verify the app was launched again after accepting the intent picker.
  auto* launched_app_browser = browser_created_observer.Wait();
  ASSERT_TRUE(web_app::AppBrowserController::IsWebApp(launched_app_browser));
  auto* launched_app_web_contents =
      launched_app_browser->tab_strip_model()->GetActiveWebContents();

  // Validate JS results.
  EXPECT_TRUE(ResultExists(launched_app_web_contents));
  EXPECT_FALSE(ErrorExists(launched_app_web_contents));

  // Validate browser results.
  histograms.ExpectUniqueSample("WebApp.LaunchSource",
                                apps::LaunchSource::kFromReparenting, 2);
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kSuccessAlreadyInstalled,
      1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTest,
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

  ASSERT_TRUE(TryInstallApp());

  // Validate JS results.
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());

  // Validate browser results.
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kSuccessAlreadyInstalled,
      1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

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

  // Validate browser results.
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kSuccessAlreadyInstalled,
      1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

// Tests for WebAppInstallNotSupportedDialog appearing in Incognito and Guest
// modes since web app installs are not supported in these modes. The dialog
// appears for all current and background document installs.

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTest,
                       NotSupportedDialogInIncognito) {
  // Open incognito window and navigate to a valid URL.
  GURL test_url = https_server()->GetURL("/simple.html");
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), test_url);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histograms;

  // Trigger the Install Not Supported dialog by initiating an install request.
  ExecuteScriptAsync(incognito_web_contents,
                     "navigator.install()"
                     ".then(result => {"
                     "  webInstallResult = result;"
                     "}).catch(error => {"
                     "  webInstallError = error;"
                     "});");

  // Wait for the dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  // Verify dialog title for Incognito mode.
  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installs aren't supported in Incognito mode");

  // Simulate the user accepting the dialog.
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  // Validate JS results.
  EXPECT_FALSE(ResultExists(incognito_web_contents));
  EXPECT_TRUE(ErrorExists(incognito_web_contents));
  EXPECT_EQ(GetErrorName(incognito_web_contents), kAbortError);

  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kUnsupportedProfile, 1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTest,
                       NotSupportedDialogAfterTabSwitching) {
  // Open incognito window and navigate to a valid URL.
  GURL test_url = https_server()->GetURL("/simple.html");
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), test_url);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histograms;

  // Trigger the Install Not Supported dialog by initiating an install request.
  ExecuteScriptAsync(incognito_web_contents,
                     "navigator.install()"
                     ".then(result => {"
                     "  webInstallResult = result;"
                     "}).catch(error => {"
                     "  webInstallError = error;"
                     "});");

  // Wait for the dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  // Change focus to a new tab.
  chrome::NewTab(incognito_browser);

  // Validate JS results.
  EXPECT_FALSE(ResultExists(incognito_web_contents));
  EXPECT_TRUE(ErrorExists(incognito_web_contents));
  EXPECT_EQ(GetErrorName(incognito_web_contents), kAbortError);

  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kUnsupportedProfile, 1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

class WebInstallGuestModeTest : public WebInstallCurrentDocumentBrowserTest {
 public:
  WebInstallGuestModeTest() = default;
  WebInstallGuestModeTest(const WebInstallGuestModeTest&) = delete;
  WebInstallGuestModeTest& operator=(const WebInstallGuestModeTest&) = delete;

#if BUILDFLAG(IS_CHROMEOS)
  // To create a guest session in ChromeOS, CreateGuestBrowser() cannot be used
  // and proper switches to commandline need to be set.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

IN_PROC_BROWSER_TEST_F(WebInstallGuestModeTest, NotSupportedDialogInGuestMode) {
  // Open a new guest mode window.
#if BUILDFLAG(IS_CHROMEOS)
  Browser* guest_browser = browser();
#else
  Browser* guest_browser = CreateGuestBrowser();
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(guest_browser->profile()->IsGuestSession());

  // Navigate to a valid URL in the guest browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      guest_browser, https_server()->GetURL("/simple.html")));

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");
  content::WebContents* guest_web_contents =
      guest_browser->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histograms;

  // Trigger the Install Not Supported dialog by initiating an install request.
  ExecuteScriptAsync(guest_web_contents,
                     "navigator.install()"
                     ".then(result => {"
                     "  webInstallResult = result;"
                     "}).catch(error => {"
                     "  webInstallError = error;"
                     "});");

  // Confirm Install Not Supported Dialog shows.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  // Verify dialog title for Guest mode.
  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installs aren't supported in Guest mode");

  // Simulate the user accepting the dialog.
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  // Validate JS results.
  EXPECT_FALSE(ResultExists(guest_web_contents));
  EXPECT_TRUE(ErrorExists(guest_web_contents));
  EXPECT_EQ(GetErrorName(guest_web_contents), kAbortError);

  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kUnsupportedProfile, 1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

// Manifest validation for current document installs.
using WebInstallCurrentDocumentBrowserTestManifestErrors =
    WebInstallCurrentDocumentBrowserTest;

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTestManifestErrors,
                       NoManifest) {
  GURL current_doc_url =
      https_server()->GetURL("/banners/no_manifest_test_page.html");

  NavigateAndConfigureCurrentDocumentForInstall(current_doc_url);
  base::HistogramTester histograms;

  ASSERT_TRUE(TryInstallApp());

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kInstallCommandFailed,
      1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

IN_PROC_BROWSER_TEST_F(WebInstallCurrentDocumentBrowserTestManifestErrors,
                       MissingId) {
  GURL current_doc_url = GetInstallableAppURL();

  NavigateAndConfigureCurrentDocumentForInstall(current_doc_url);
  base::HistogramTester histograms;

  ASSERT_TRUE(TryInstallApp());

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kDataError);
  histograms.ExpectBucketCount(
      kInstallResultUma, web_app::WebInstallApiResult::kNoCustomManifestId, 1);
  histograms.ExpectBucketCount(kInstallTypeUma,
                               web_app::WebInstallApiType::kCurrentDocument, 1);
}

// Implementation-generic tests for bad JavaScript API inputs. This failure
// handling is on the blink side, so there aren't any browser results to verify.
using WebInstallServiceImplBrowserTestBadInput =
    WebInstallCurrentDocumentBrowserTest;

IN_PROC_BROWSER_TEST_F(WebInstallServiceImplBrowserTestBadInput,
                       MissingUserGesture) {
  NavigateToValidUrl();

  std::string install_url = GetInstallableAppURL().spec();
  std::string manifest_id = install_url;
  ASSERT_TRUE(TryInstallApp(/*with_gesture=*/false));

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
