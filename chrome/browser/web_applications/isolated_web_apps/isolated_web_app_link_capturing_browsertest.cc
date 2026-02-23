// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_navigation_capturing_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/signing_keys.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::ManifestLaunchHandler_ClientMode;

namespace web_app {

namespace {

constexpr std::string_view kIwaHtmlContent = R"(
  <html>
  <script src="script.js">
  </script>
  <body>
    <h1>IWA Navigation Capture Test</h1>
    <pre id="message">Launch params received:</pre>
  </body>
  </html>
)";

constexpr std::string_view kIwaJsContent = R"(
  console.log('Setting up the launch queue');
  var launchParamsTargetUrls = [];
  function recordLaunchParam(url) {
    launchParamsTargetUrls.push(url);
  }
  window.launchQueue.setConsumer((launchParams) => {
    recordLaunchParam(launchParams.targetURL);
  });
)";

std::string OriginAssociationFileFromAppIdentity(std::string iwa_bundle_id) {
  return *base::WriteJson(base::DictValue().Set(
      base::StringPrintf("isolated-app://%s/", iwa_bundle_id.c_str()),
      base::DictValue().Set("scope",
                            "/web_apps/intent_picker_nav_capture/index.html")));
}

std::string ClientModeToString(ManifestLaunchHandler_ClientMode mode) {
  switch (mode) {
    case ManifestLaunchHandler_ClientMode::kFocusExisting:
      return "FocusExisting";
    case ManifestLaunchHandler_ClientMode::kNavigateExisting:
      return "NavigateExisting";
    case ManifestLaunchHandler_ClientMode::kAuto:
    case ManifestLaunchHandler_ClientMode::kNavigateNew:
      return "NavigateNew";
  }
}

enum class IwaInitialState { kClosed, kOpen };

enum class OpenScriptWay { kLinkClickTargetBlank, kServerRedirect };

std::string GenerateTestName(
    const testing::TestParamInfo<std::tuple<ManifestLaunchHandler_ClientMode,
                                            IwaInitialState,
                                            OpenScriptWay>>& info) {
  std::string mode_str = ClientModeToString(std::get<0>(info.param));

  std::string open_str;
  switch (std::get<1>(info.param)) {
    case IwaInitialState::kOpen:
      open_str = "InitiallyOpen";
      break;
    case IwaInitialState::kClosed:
      open_str = "InitiallyClosed";
      break;
  }

  std::string script_str;
  switch (std::get<2>(info.param)) {
    case OpenScriptWay::kLinkClickTargetBlank:
      script_str = "LinkClickTargetBlank";
      break;
    case OpenScriptWay::kServerRedirect:
      script_str = "ServerRedirect";
      break;
  }

  return mode_str + "_" + open_str + "_" + script_str;
}

std::string GenerateAppWindowTestName(
    const testing::TestParamInfo<ManifestLaunchHandler_ClientMode>& info) {
  return ClientModeToString(info.param);
}

}  // namespace

class IsolatedWebAppLinkCapturingBrowserTestBase
    : public WebAppNavigationCapturingBrowserTestBase {
 public:
  IsolatedWebAppLinkCapturingBrowserTestBase() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebAppEnableScopeExtensionsForIsolatedWebApps
#if !BUILDFLAG(IS_CHROMEOS)
         ,
         features::kIsolatedWebApps
#endif  // !BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }

  void SetUpOnMainThread() override {
    WebAppNavigationCapturingBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  GURL GetCapturableUrlWithQuery() {
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

  void InstallIsolatedWebApp(ManifestLaunchHandler_ClientMode client_mode) {
    const auto bundle_id = web_package::SignedWebBundleId::CreateForPublicKey(
        web_package::test::GetDefaultEd25519KeyPair().public_key);

    const url::Origin scope_extended_origin =
        url::Origin::Create(GetCapturableUrlWithQuery());
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

    app_id_ = url_info.app_id();
    app_start_url_ = url_info.origin().GetURL();
  }

  // Simulates a click on a specific element ID in the given web contents.
  // Supports modifiers (shift, ctrl, etc) and mouse buttons (middle, left).
  void SimulateClickOnElement(content::WebContents* contents,
                              std::string element_id,
                              blink::WebInputEvent::Modifiers modifiers,
                              blink::WebMouseEvent::Button button) {
    // Get the center coordinates of the element.
    std::string script = base::StringPrintf(
        R"(
          const rect = document.getElementById('%s').getBoundingClientRect();
          [rect.x + rect.width / 2, rect.y + rect.height / 2];
        )",
        element_id.c_str());

    content::EvalJsResult eval_js_result = content::EvalJs(contents, script);
    const base::ListValue& result = eval_js_result.ExtractList();
    double x = result[0].GetDouble();
    double y = result[1].GetDouble();

    // Simulate the click at those coordinates.
    content::SimulateMouseClickAt(contents, modifiers, button,
                                  gfx::Point(x, y));
  }

  const webapps::AppId& app_id() const { return app_id_; }
  const GURL& app_start_url() const { return app_start_url_; }

  content::WebContents* GetBrowserTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  webapps::AppId app_id_;
  GURL app_start_url_;
};

// Capturing from a standard browser window.
class IsolatedWebAppLinkCapturingFromBrowserWindowBrowserTest
    : public IsolatedWebAppLinkCapturingBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<ManifestLaunchHandler_ClientMode,
                     IwaInitialState,
                     OpenScriptWay>> {
 public:
  void SetUpOnMainThread() override {
    IsolatedWebAppLinkCapturingBrowserTestBase::SetUpOnMainThread();
    InstallIsolatedWebApp(GetClientMode());
  }

 protected:
  ManifestLaunchHandler_ClientMode GetClientMode() const {
    return std::get<0>(GetParam());
  }

  bool IsIwaInitiallyOpened() const {
    return std::get<1>(GetParam()) == IwaInitialState::kOpen;
  }

  OpenScriptWay GetOpenScriptWay() const { return std::get<2>(GetParam()); }
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppLinkCapturingFromBrowserWindowBrowserTest,
                       NavigationCapture) {
  // Open IWA if required.
  Browser* existing_app_browser = nullptr;
  content::WebContents* existing_app_contents = nullptr;

  if (IsIwaInitiallyOpened()) {
    content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
    existing_app_contents = content::WebContents::FromRenderFrameHost(frame);
    existing_app_browser = chrome::FindBrowserWithTab(existing_app_contents);

    // Verify initial state launch params.
    WaitForLaunchParams(existing_app_contents,
                        /*min_launch_params_to_wait_for=*/1);
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    existing_app_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(app_start_url()));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/simple.html")));

  ASSERT_TRUE(
      apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id())
          .has_value());

  GURL destination_url = GetCapturableUrlWithQuery();
  std::string script;

  switch (GetOpenScriptWay()) {
    case OpenScriptWay::kLinkClickTargetBlank:
      script = base::StringPrintf(
          R"(
            const link = document.createElement('a');
            link.href = '%s';
            link.target = '_blank';
            link.textContent = 'Click me';
            document.body.append(link);
            link.click();
          )",
          destination_url.spec().c_str());
      break;

    case OpenScriptWay::kServerRedirect:
      GURL redirect_url =
          https_server()->GetURL("/server-redirect?" + destination_url.spec());
      script = base::StringPrintf(
          R"(
            const link = document.createElement('a');
            link.href = '%s';
            link.target = '_blank';
            link.textContent = 'Redirect me';
            document.body.append(link);
            link.click();
          )",
          redirect_url.spec().c_str());
      break;
  }

  bool expect_new_window =
      GetClientMode() == ManifestLaunchHandler_ClientMode::kNavigateNew ||
      !IsIwaInitiallyOpened();

  // We use BrowserCreatedObserver because we expect a new IWA window.
  // We avoid AllBrowserTabAddedWaiter because it might catch intermediate tabs
  // created by the renderer (e.g. during redirects or target=_blank) before
  // capturing occurs.
  ui_test_utils::BrowserCreatedObserver browser_observer;

  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  if (expect_new_window) {
    Browser* new_browser = browser_observer.Wait();
    ASSERT_TRUE(new_browser);
    EXPECT_NE(new_browser, browser());
    EXPECT_NE(new_browser, existing_app_browser);
    EXPECT_TRUE(AppBrowserController::IsForWebApp(new_browser, app_id()));

    content::WebContents* new_contents =
        new_browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(new_contents);
    EXPECT_TRUE(content::WaitForLoadStop(new_contents));

    // Verify Launch Params in new window.
    WaitForLaunchParams(new_contents, /*min_launch_params_to_wait_for=*/1);
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    new_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(destination_url));

    // Verify the URL committed is the IWA start URL.
    EXPECT_EQ(new_contents->GetLastCommittedURL(), app_start_url());

  } else {
    // Expecting reuse of existing window.
    ASSERT_TRUE(existing_app_browser)
        << "Expected reuse, but no existing app browser.";
    ASSERT_TRUE(existing_app_contents);

    // Verify params increased in existing window.
    WaitForLaunchParams(existing_app_contents,
                        /*min_launch_params_to_wait_for=*/2);
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    existing_app_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(app_start_url(), destination_url));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    IsolatedWebAppLinkCapturingFromBrowserWindowBrowserTest,
    testing::Combine(
        testing::Values(ManifestLaunchHandler_ClientMode::kFocusExisting,
                        ManifestLaunchHandler_ClientMode::kNavigateExisting,
                        ManifestLaunchHandler_ClientMode::kNavigateNew),
        testing::Values(IwaInitialState::kClosed, IwaInitialState::kOpen),
        testing::Values(OpenScriptWay::kLinkClickTargetBlank,
                        OpenScriptWay::kServerRedirect)),
    GenerateTestName);

// Capturing from inside the App Window
class IsolatedWebAppLinkCapturingFromAppWindowBrowserTest
    : public IsolatedWebAppLinkCapturingBrowserTestBase,
      public testing::WithParamInterface<ManifestLaunchHandler_ClientMode> {
 public:
  void SetUpOnMainThread() override {
    IsolatedWebAppLinkCapturingBrowserTestBase::SetUpOnMainThread();
    InstallIsolatedWebApp(GetClientMode());
  }

 protected:
  ManifestLaunchHandler_ClientMode GetClientMode() const { return GetParam(); }

  void CreateLinkInTab(content::WebContents* web_content,
                       const GURL& url,
                       const std::string& id,
                       const std::string& target = "") {
    std::string script = base::StringPrintf(
        R"(
          const link = document.createElement('a');
          link.id = '%s';
          link.href = '%s';
          link.textContent = 'Capturable Link';
          if ('%s') {
            link.target = '%s';
          }
          document.body.append(link);
        )",
        id.c_str(), url.spec().c_str(), target.c_str(), target.c_str());
    ASSERT_TRUE(content::ExecJs(web_content, script));
  }
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppLinkCapturingFromAppWindowBrowserTest,
                       MiddleClickOpensNewBrowserTab) {
  // Open IWA initially (Window 1).
  content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
  content::WebContents* existing_app_contents =
      content::WebContents::FromRenderFrameHost(frame);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);
  ASSERT_TRUE(
      apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id())
          .has_value());

  int initial_browser_tabs_count = browser()->tab_strip_model()->count();

  // Middle Click the link.
  GURL destination_url = GetCapturableUrlWithQuery();
  CreateLinkInTab(existing_app_contents, destination_url, "capture-link");

  apps::test::NavigationCommittedForUrlObserver load_observer(destination_url);
  SimulateClickOnElement(existing_app_contents, "capture-link",
                         blink::WebInputEvent::kNoModifiers,
                         blink::WebMouseEvent::Button::kMiddle);

  // Verify browser tab is opened.
  load_observer.Wait();
  content::WebContents* new_tab = load_observer.web_contents();

  ASSERT_EQ(initial_browser_tabs_count + 1,
            browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      web_app::WebAppTabHelper::FromWebContents(new_tab)->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppLinkCapturingFromAppWindowBrowserTest,
                       ShiftClickOpensNewAppWindow) {
  // Open IWA initially (Window 1).
  content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
  content::WebContents* existing_app_contents =
      content::WebContents::FromRenderFrameHost(frame);
  Browser* existing_app_browser =
      chrome::FindBrowserWithTab(existing_app_contents);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);
  ASSERT_TRUE(
      apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id())
          .has_value());

  // Shift + Left Click the link.
  GURL destination_url = GetCapturableUrlWithQuery();
  CreateLinkInTab(existing_app_contents, destination_url, "capture-link");

  ui_test_utils::BrowserCreatedObserver browser_observer;
  SimulateClickOnElement(existing_app_contents, "capture-link",
                         blink::WebInputEvent::kShiftKey,
                         blink::WebMouseEvent::Button::kLeft);

  // Verify NEW window opened regardless of ClientMode.
  Browser* new_browser = browser_observer.Wait();
  ASSERT_TRUE(new_browser);
  EXPECT_NE(new_browser, browser());
  EXPECT_NE(new_browser, existing_app_browser);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(new_browser, app_id()));

  content::WebContents* new_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  WaitForLaunchParams(new_contents, /*min_launch_params_to_wait_for=*/1);
  EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                  new_contents, "launchParamsTargetUrls"),
              testing::ElementsAre(destination_url));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppLinkCapturingFromAppWindowBrowserTest,
                       NormalClickRespondsToClientMode) {
  // Open IWA initially (Window 1).
  content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
  content::WebContents* existing_app_contents =
      content::WebContents::FromRenderFrameHost(frame);
  Browser* existing_app_browser =
      chrome::FindBrowserWithTab(existing_app_contents);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);
  ASSERT_TRUE(
      apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id())
          .has_value());

  // Usual click on the link.
  GURL destination_url = GetCapturableUrlWithQuery();
  CreateLinkInTab(existing_app_contents, destination_url, "capture-link");

  bool expect_new_window =
      GetClientMode() == ManifestLaunchHandler_ClientMode::kNavigateNew;

  ui_test_utils::BrowserCreatedObserver browser_observer;
  SimulateClickOnElement(existing_app_contents, "capture-link",
                         blink::WebInputEvent::kNoModifiers,
                         blink::WebMouseEvent::Button::kLeft);

  if (expect_new_window) {
    Browser* new_browser = browser_observer.Wait();
    ASSERT_TRUE(new_browser);
    EXPECT_NE(new_browser, existing_app_browser);
    EXPECT_TRUE(AppBrowserController::IsForWebApp(new_browser, app_id()));

    content::WebContents* new_contents =
        new_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(new_contents));

    WaitForLaunchParams(new_contents, /*min_launch_params_to_wait_for=*/1);
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    new_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(destination_url));
  } else {
    WaitForLaunchParams(existing_app_contents,
                        /*min_launch_params_to_wait_for=*/2);
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    existing_app_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(app_start_url(), destination_url));
  }
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppLinkCapturingFromAppWindowBrowserTest,
                       TargetBlankClickOpensAppWindow) {
  // Open IWA initially (Window 1).
  content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
  content::WebContents* existing_app_contents =
      content::WebContents::FromRenderFrameHost(frame);
  Browser* existing_app_browser =
      chrome::FindBrowserWithTab(existing_app_contents);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);
  ASSERT_TRUE(
      apps::test::EnableLinkCapturingByUser(browser()->profile(), app_id())
          .has_value());

  // Create link with target="_blank".
  GURL destination_url = GetCapturableUrlWithQuery();
  CreateLinkInTab(existing_app_contents, destination_url, "capture-link",
                  /*target=*/"_blank");

  ui_test_utils::BrowserCreatedObserver browser_observer;
  SimulateClickOnElement(existing_app_contents, "capture-link",
                         blink::WebInputEvent::kNoModifiers,
                         blink::WebMouseEvent::Button::kLeft);

  Browser* new_browser = browser_observer.Wait();
  ASSERT_TRUE(new_browser);
  EXPECT_NE(new_browser, existing_app_browser);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(new_browser, app_id()));

  content::WebContents* new_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  WaitForLaunchParams(new_contents, /*min_launch_params_to_wait_for=*/1);
  EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                  new_contents, "launchParamsTargetUrls"),
              testing::ElementsAre(destination_url));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    IsolatedWebAppLinkCapturingFromAppWindowBrowserTest,
    testing::Values(ManifestLaunchHandler_ClientMode::kFocusExisting,
                    ManifestLaunchHandler_ClientMode::kNavigateExisting,
                    ManifestLaunchHandler_ClientMode::kNavigateNew),
    GenerateAppWindowTestName);

}  // namespace web_app
