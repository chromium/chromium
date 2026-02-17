// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_navigation_capturing_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/signing_keys.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "url/gurl.h"

using blink::mojom::ManifestLaunchHandler_ClientMode;

namespace web_app {

class WebAppNavigationCapturingIntentPickerBrowserTest
    : public WebAppNavigationCapturingBrowserTestBase {
 protected:
  GURL GetAppUrl() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/index.html");
  }

  GURL GetAppUrlWithQuery() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/"
        "index.html?q=fake_query_to_check_navigation");
  }

  GURL GetAppUrlWithWCO() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/index_wco.html");
  }
};

// TODO(crbug.com/376641667): Flaky on Mac & Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_FocusExisting DISABLED_FocusExisting
#else
#define MAYBE_FocusExisting FocusExisting
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       MAYBE_FocusExisting) {
  webapps::AppId app_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateForTesting(
                     GetAppUrl(), blink::mojom::DisplayMode::kMinimalUi,
                     mojom::UserDisplayMode::kStandalone,
                     ManifestLaunchHandler_ClientMode::kFocusExisting));

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_NE(app_browser, browser());
  content::WebContents* app_contents =
      app_browser->tab_strip_model()->GetWebContentsAt(0);

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), GetAppUrlWithQuery());
  EXPECT_NE(nullptr, host);

  // Warning: A `tab_contents` pointer obtained from browser() will be invalid
  // after calling this function.
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));

  WaitForLaunchParams(app_contents, /* min_launch_params_to_wait_for= */ 2);

  // Check the end state for the browser() -- it should have survived the Intent
  // Picker action.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Check the end state for the app.
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(app_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            GetAppUrl());

  std::vector<GURL> launch_params = apps::test::GetLaunchParamUrlsInContents(
      app_contents, "launchParamsTargetUrls");
  // There should be two launch params -- one for the initial launch and one
  // for when the existing app got focus (via the Intent Picker) and launch
  // params were enqueued.
  EXPECT_THAT(launch_params,
              testing::ElementsAre(GetAppUrl(), GetAppUrlWithQuery()));
}

// TODO(crbug.com/382315984): Fix this flake.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NavigateExisting DISABLED_NavigateExisting
#else
#define MAYBE_NavigateExisting NavigateExisting
#endif

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       MAYBE_NavigateExisting) {
  webapps::AppId app_id = InstallWebApp(WebAppInstallInfo::CreateForTesting(
      GetAppUrl(), blink::mojom::DisplayMode::kMinimalUi,
      mojom::UserDisplayMode::kStandalone,
      ManifestLaunchHandler_ClientMode::kNavigateExisting));

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_NE(app_browser, browser());
  content::WebContents* app_contents =
      app_browser->tab_strip_model()->GetWebContentsAt(0);

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), GetAppUrlWithQuery());
  EXPECT_NE(nullptr, host);

  // Warning: A `tab_contents` pointer obtained from browser() will be invalid
  // after calling this function.
  ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));

  WaitForLaunchParams(app_contents,
                      /* min_launch_params_to_wait_for= */ 1);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(app_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            GetAppUrlWithQuery());

  std::vector<GURL> launch_params = apps::test::GetLaunchParamUrlsInContents(
      app_contents, "launchParamsTargetUrls");
  // There should be one launch param -- because the Intent Picker triggers a
  // new navigation in the app (and launch params are then enqueued).
  EXPECT_THAT(launch_params, testing::ElementsAre(GetAppUrlWithQuery()));
}

// Test that the intent picker shows up for chrome://password-manager, since it
// is installable.
IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       DoShowIconAndBubbleOnChromePasswordManagerPage) {
  GURL password_manager_url("chrome://password-manager");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), password_manager_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  webapps::AppId pwd_manager_app_id =
      web_app::InstallWebAppFromPageAndCloseAppBrowser(browser(),
                                                       password_manager_url);

  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), password_manager_url);
  ASSERT_NE(nullptr, host);

  ASSERT_TRUE(WaitForIntentPickerToShow(browser()));
}

IN_PROC_BROWSER_TEST_F(WebAppNavigationCapturingIntentPickerBrowserTest,
                       VerifyWindowControlsOverlayReappears) {
  auto ensure_app_browser =
      [&](base::FunctionRef<webapps::AppId()> app_browser_launcher) {
        ui_test_utils::BrowserCreatedObserver browser_created_observer;
        webapps::AppId app_id = app_browser_launcher();
        Browser* app_browser = browser_created_observer.Wait();
        EXPECT_NE(app_browser, browser());
        EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
        return std::make_pair(app_browser, app_id);
      };

  // Install WCO app and toggle the Window Controls Overlay display.
  std::pair<Browser*, webapps::AppId> install_data = ensure_app_browser(
      [&] { return InstallWebAppFromPage(browser(), GetAppUrlWithWCO()); });
  Browser* app_browser = install_data.first;
  const webapps::AppId app_id = install_data.second;

  // Toggle the Window Controls Overlay display in the current app_browser so
  // that the behavior is stored.
  base::test::TestFuture<void> test_future;
  content::WebContents* contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher1(contents, u"WCO Enabled");
  app_browser->GetBrowserView().ToggleWindowControlsOverlayEnabled(
      test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  std::ignore = title_watcher1.WaitAndGetTitle();
  ASSERT_TRUE(app_browser->GetBrowserView().IsWindowControlsOverlayEnabled());

  // Disable navigation capturing for the app_id so that the enable link
  // capturing infobar shows up.
  ASSERT_EQ(apps::test::DisableLinkCapturingByUser(profile(), app_id),
            base::ok());

  // Navigate to the WCO app site, verify the intent picker icon shows up.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrlWithWCO()));
  ASSERT_TRUE(web_app::WaitForIntentPickerToShow(browser()));
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, nullptr);

  // Click on the intent picker icon, and verify an app browser gets launched
  // with no WCO. The link capturing infobar is shown.
  // `include_nestable_tasks` is set to true because the "default" RunLoop
  // hangs, waiting for certain nested tasks to finish.
  content::TitleWatcher title_watcher2(new_contents, u"WCO Disabled",
                                       /*include_nestable_tasks=*/true);
  std::pair<Browser*, webapps::AppId> post_intent_picker_data =
      ensure_app_browser([&] {
        EXPECT_TRUE(web_app::ClickIntentPickerChip(browser()));
        return app_id;
      });
  Browser* new_app_browser = post_intent_picker_data.first;
  std::ignore = title_watcher2.WaitAndGetTitle();
  EXPECT_FALSE(
      new_app_browser->GetBrowserView().IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(
      apps::EnableLinkCapturingInfoBarDelegate::FindInfoBar(new_contents));

  // Close the infobar, and wait for the WCO to come back on.
  // `include_nestable_tasks` is set to true because the "default" RunLoop
  // hangs, waiting for certain nested tasks to finish.
  content::TitleWatcher title_watcher3(new_contents, u"WCO Enabled",
                                       /*include_nestable_tasks=*/true);
  apps::EnableLinkCapturingInfoBarDelegate::RemoveInfoBar(new_contents);
  std::ignore = title_watcher3.WaitAndGetTitle();
  EXPECT_TRUE(
      new_app_browser->GetBrowserView().IsWindowControlsOverlayEnabled());
  EXPECT_FALSE(
      apps::EnableLinkCapturingInfoBarDelegate::FindInfoBar(new_contents));
}

// -----------------------------------------------------------------------------
// Isolated Web App Tests
// -----------------------------------------------------------------------------

namespace {
constexpr std::string_view kIwaHtmlContent = R"(
  <html>
  <script src="script.js">
  </script>
  <body>
    <h1>Intent Picker Navigation Capture test</h1>
    <pre id="message">Launch params received:</pre>
  </body>
  </html>
)";

constexpr std::string_view kIwaJsContent = R"(
  console.log('Setting up the launch queue');
  var launchParamsTargetUrls = [];
  function recordLaunchParam(url) {
    // Display the launch param received.
    launchParamsTargetUrls.push(url);
  }
  window.launchQueue.setConsumer((launchParams) => {
    recordLaunchParam(launchParams.targetURL);
  });
)";

std::string OriginAssociationFileFromAppIdentity(std::string iwa_bundle_id) {
  return *base::WriteJson(base::DictValue().Set(
      base::StringPrintf("isolated-app://%s/", iwa_bundle_id),
      base::DictValue().Set("scope",
                            "/web_apps/intent_picker_nav_capture/index.html")));
}

enum class IwaInitialState { kClosed, kOpen };

std::string GenerateTestName(
    const testing::TestParamInfo<
        std::tuple<ManifestBuilder::ClientMode, IwaInitialState>>& info) {
  std::string mode_str;
  switch (std::get<0>(info.param)) {
    case ManifestBuilder::ClientMode::kFocusExisting:
      mode_str = "FocusExisting";
      break;
    case ManifestBuilder::ClientMode::kNavigateExisting:
      mode_str = "NavigateExisting";
      break;
    case ManifestBuilder::ClientMode::kAuto:
    case ManifestBuilder::ClientMode::kNavigateNew:
      mode_str = "NavigateNew";
      break;
  }
  std::string open_str;
  switch (std::get<1>(info.param)) {
    case IwaInitialState::kOpen:
      open_str = "InitiallyOpen";
      break;
    case IwaInitialState::kClosed:
      open_str = "InitiallyClosed";
      break;
  }

  return mode_str + "_" + open_str;
}

}  // namespace

class IsolatedWebAppNavigationCapturingIntentPickerBrowserTest
    : public WebAppNavigationCapturingBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<ManifestBuilder::ClientMode, IwaInitialState>> {
 public:
  IsolatedWebAppNavigationCapturingIntentPickerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebAppEnableScopeExtensionsForIsolatedWebApps,
#if !BUILDFLAG(IS_CHROMEOS)
         features::kIsolatedWebApps
#endif  // !BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }

 protected:
  ManifestBuilder::ClientMode GetClientMode() const {
    return std::get<0>(GetParam());
  }

  bool IsIwaInitiallyOpened() const {
    return std::get<1>(GetParam()) == IwaInitialState::kOpen;
  }

  GURL GetAppUrl() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/index.html");
  }

  GURL GetAppUrlWithQuery() {
    return https_server()->GetURL(
        "/web_apps/intent_picker_nav_capture/"
        "index.html?q=fake_query_to_check_navigation");
  }

  void SetFakeOriginAssociationFetcher(
      url::Origin request_origin,
      const web_package::SignedWebBundleId& bundle_id) {
    auto origin_association_fetcher =
        std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();

    origin_association_fetcher->SetData(
        {{std::move(request_origin),
          OriginAssociationFileFromAppIdentity(bundle_id.id())}});

    provider().origin_association_manager().SetFetcherForTest(
        std::move(origin_association_fetcher));
  }

  IsolatedWebAppUrlInfo InstallIsolatedWebApp(
      ManifestBuilder::ClientMode client_mode) {
    const auto bundle_id = web_package::SignedWebBundleId::CreateForPublicKey(
        web_package::test::GetDefaultEd25519KeyPair().public_key);

    const url::Origin scope_extended_origin = url::Origin::Create(GetAppUrl());
    SetFakeOriginAssociationFetcher(scope_extended_origin, bundle_id);

    IsolatedWebAppUrlInfo url_info =
        IsolatedWebAppBuilder(
            ManifestBuilder()
                .SetLaunchHandlerClientMode(client_mode)
                .AddScopeExtension(scope_extended_origin,
                                   /*has_origin_wildcard=*/false))
            .AddHtml("/", kIwaHtmlContent)
            .AddJs("script.js", kIwaJsContent)
            .BuildBundle(bundle_id,
                         {web_package::test::GetDefaultEd25519KeyPair()})
            ->InstallChecked(browser()->profile());

    return url_info;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/481793608): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_LaunchParams DISABLED_LaunchParams
#else
#define MAYBE_LaunchParams LaunchParams
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(IsolatedWebAppNavigationCapturingIntentPickerBrowserTest,
                       MAYBE_LaunchParams) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(GetClientMode());
  content::WebContents* first_app_contents = nullptr;

  // Conditionally Launch First Instance.
  if (IsIwaInitiallyOpened()) {
    first_app_contents = content::WebContents::FromRenderFrameHost(
        OpenIsolatedWebApp(profile(), url_info.app_id()));

    // Verify Window 1 received its initial launch param.
    WaitForLaunchParams(first_app_contents,
                        /*min_launch_params_to_wait_for=*/1);
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    first_app_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(url_info.origin().GetURL()));
  }

  // Navigate Main Browser to capturable URL.
  content::RenderFrameHost* host =
      ui_test_utils::NavigateToURL(browser(), GetAppUrlWithQuery());
  ASSERT_NE(nullptr, host);

  // Verify the Intent Picker appears.
  EXPECT_TRUE(web_app::WaitForIntentPickerToShow(browser()));

  bool expect_new_window =
      GetClientMode() == ManifestBuilder::ClientMode::kNavigateNew ||
      !IsIwaInitiallyOpened();

  if (expect_new_window) {
    ui_test_utils::BrowserCreatedObserver browser_observer;
    ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));
    Browser* second_app_browser = browser_observer.Wait();

    // Verify the new browser is for the correct app.
    EXPECT_TRUE(AppBrowserController::IsForWebApp(second_app_browser,
                                                  url_info.app_id()));
    EXPECT_NE(second_app_browser, browser());

    // If we had an initial window, ensure the new one is distinct.
    if (first_app_contents) {
      EXPECT_NE(second_app_browser->tab_strip_model()->GetWebContentsAt(0),
                first_app_contents);
    }

    // Verify Launch Params in the NEW window.
    content::WebContents* second_app_contents =
        second_app_browser->tab_strip_model()->GetActiveWebContents();
    WaitForLaunchParams(second_app_contents,
                        /*min_launch_params_to_wait_for=*/1);

    // The new window should only see the captured URL as its launch param.
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    second_app_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(GetAppUrlWithQuery()));

    // If Window 1 existed, it should NOT have received new params.
    if (first_app_contents) {
      EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                      first_app_contents, "launchParamsTargetUrls"),
                  testing::ElementsAre(url_info.origin().GetURL()));
    }

  } else {
    // Expectation: Reuse Existing Window.
    // This path is only taken if IsIwaInitiallyOpened() is true AND
    // ClientMode is NOT kNavigateNew.

    ASSERT_TRUE(first_app_contents);
    ASSERT_TRUE(web_app::ClickIntentPickerChip(browser()));

    WaitForLaunchParams(first_app_contents,
                        /* min_launch_params_to_wait_for= */ 2);

    // Browser check: should have survived.
    EXPECT_EQ(1, browser()->tab_strip_model()->count());

    // Window 1 should have received the NEW param appended to the old one.
    EXPECT_THAT(
        apps::test::GetLaunchParamUrlsInContents(first_app_contents,
                                                 "launchParamsTargetUrls"),
        testing::ElementsAre(url_info.origin().GetURL(), GetAppUrlWithQuery()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppNavigationCapturingIntentPickerBrowserTest,
    testing::Combine(
        testing::Values(ManifestBuilder::ClientMode::kFocusExisting,
                        ManifestBuilder::ClientMode::kNavigateExisting,
                        ManifestBuilder::ClientMode::kNavigateNew),
        testing::Values(IwaInitialState::kClosed, IwaInitialState::kOpen)),
    GenerateTestName);

}  // namespace web_app
