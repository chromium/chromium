// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_navigation_capturing_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
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
    auto origin_association_fetcher =
        std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();

    fake_origin_association_fetcher_ = origin_association_fetcher.get();

    provider().origin_association_manager().SetFetcherForTest(
        std::move(origin_association_fetcher));

    clock_.SetNow(base::Time::Now());
    provider().SetClockForTesting(&clock_);
  }

  void TearDownOnMainThread() override {
    fake_origin_association_fetcher_ = nullptr;
    WebAppNavigationCapturingBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  GURL GetCapturableUrlWithQuery() {
    return embedded_https_test_server().GetURL(
        "/web_apps/intent_picker_nav_capture/"
        "index.html?q=fake_query_to_check_navigation");
  }

  GURL GetCapturableUrl() {
    return embedded_https_test_server().GetURL(
        "/web_apps/intent_picker_nav_capture/"
        "index.html");
  }

  void SetFakeOriginAssociationFetcherData(
      const url::Origin& origin,
      const web_package::SignedWebBundleId& bundle_id) {
    fake_origin_association_fetcher_->SetData(
        {{origin, OriginAssociationFileFromAppIdentity(bundle_id.id())}});
  }

  IsolatedWebAppUrlInfo InstallIsolatedWebApp(
      ManifestLaunchHandler_ClientMode client_mode,
      bool with_scope_extensions = true,
      std::string version = "1.0.0") {
    auto manifest_builder = ManifestBuilder()
                                .SetName("app_" + version)
                                .SetVersion(version)
                                .SetLaunchHandlerClientMode(client_mode);

    if (with_scope_extensions) {
      manifest_builder.AddScopeExtension(
          url::Origin::Create(GetCapturableUrlWithQuery()),
          /*has_origin_wildcard=*/false);
    }
    IsolatedWebAppUrlInfo url_info =
        IsolatedWebAppBuilder(std::move(manifest_builder))
            .AddHtml("/", kIwaHtmlContent)
            .AddJs("script.js", kIwaJsContent)
            .BuildBundle(GetBundleId(),
                         {web_package::test::GetDefaultEd25519KeyPair()})
            ->InstallChecked(browser()->profile());

    app_id_ = url_info.app_id();
    app_start_url_ = url_info.origin().GetURL();

    return url_info;
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

  const webapps::AppId& app_id() const { return app_id_; }
  const GURL& app_start_url() const { return app_start_url_; }
  base::SimpleTestClock& clock() { return clock_; }

  const web_package::SignedWebBundleId GetBundleId() const {
    return web_package::SignedWebBundleId::CreateForPublicKey(
        web_package::test::GetDefaultEd25519KeyPair().public_key);
  }

  const webapps::TestWebAppOriginAssociationFetcher&
  fake_origin_association_fetcher() {
    return *fake_origin_association_fetcher_;
  }

  content::WebContents* GetBrowserTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  webapps::AppId app_id_;
  GURL app_start_url_;
  raw_ptr<webapps::TestWebAppOriginAssociationFetcher>
      fake_origin_association_fetcher_;
  base::SimpleTestClock clock_;
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
    SetFakeOriginAssociationFetcherData(
        url::Origin::Create(GetCapturableUrlWithQuery()), GetBundleId());
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
  BrowserWindowInterface* existing_app_browser = nullptr;
  content::WebContents* existing_app_contents = nullptr;

  if (IsIwaInitiallyOpened()) {
    content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
    existing_app_contents = content::WebContents::FromRenderFrameHost(frame);
    existing_app_browser =
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            existing_app_contents);

    // Verify initial state launch params.
    WaitForLaunchParams(existing_app_contents,
                        /*min_launch_params_to_wait_for=*/1);
    EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                    existing_app_contents, "launchParamsTargetUrls"),
                testing::ElementsAre(app_start_url()));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL("/simple.html")));

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
      GURL redirect_url = embedded_https_test_server().GetURL(
          "/server-redirect?" + destination_url.spec());
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
    SetFakeOriginAssociationFetcherData(
        url::Origin::Create(GetCapturableUrlWithQuery()), GetBundleId());
    InstallIsolatedWebApp(GetClientMode());
  }

 protected:
  ManifestLaunchHandler_ClientMode GetClientMode() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppLinkCapturingFromAppWindowBrowserTest,
                       MiddleClickOpensNewBrowserTab) {
  // Open IWA initially (Window 1).
  content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
  content::WebContents* existing_app_contents =
      content::WebContents::FromRenderFrameHost(frame);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);

  int initial_browser_tabs_count = browser()->tab_strip_model()->count();

  // Middle Click the link.
  GURL destination_url = GetCapturableUrlWithQuery();
  CreateLinkInTab(existing_app_contents, destination_url, "capture-link");

  // Wait for hit-test data to be updated so the simulated click doesn't miss.
  content::MainThreadFrameObserver(
      existing_app_contents->GetPrimaryMainFrame()->GetRenderWidgetHost())
      .Wait();

  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  SimulateClickOnElement(existing_app_contents, "capture-link",
                         blink::WebInputEvent::kNoModifiers,
                         blink::WebMouseEvent::Button::kMiddle);

  // Verify browser tab is opened.
  content::WebContents* new_tab = tab_waiter.Wait();
  content::WaitForLoadStop(new_tab);

  // Check that the new tab navigated to the correct destination.
  EXPECT_EQ(destination_url, new_tab->GetLastCommittedURL());

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
  BrowserWindowInterface* existing_app_browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          existing_app_contents);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);

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
  BrowserWindowInterface* existing_app_browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          existing_app_contents);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);

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
  BrowserWindowInterface* existing_app_browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          existing_app_contents);

  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);

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

using IsolatedWebAppLinkCapturingDefaultBehaviorBrowserTest =
    IsolatedWebAppLinkCapturingBrowserTestBase;

// Ensure that link capturing is enabled by default for Isolated Web Apps.
// Note, link capturing is disabled in ChromeOS by default for PWA.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppLinkCapturingDefaultBehaviorBrowserTest,
                       TargetBlankClick) {
  SetFakeOriginAssociationFetcherData(
      url::Origin::Create(GetCapturableUrlWithQuery()), GetBundleId());
  InstallIsolatedWebApp(ManifestLaunchHandler_ClientMode::kNavigateNew);
  // Create link with target="_blank".
  GURL destination_url = GetCapturableUrlWithQuery();
  CreateLinkInTab(GetBrowserTab(), destination_url, "capture-link",
                  /*target=*/"_blank");

  ui_test_utils::BrowserCreatedObserver browser_observer;
  SimulateClickOnElement(GetBrowserTab(), "capture-link",
                         blink::WebInputEvent::kNoModifiers,
                         blink::WebMouseEvent::Button::kLeft);

  Browser* new_browser = browser_observer.Wait();
  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(new_browser, app_id()));

  content::WebContents* new_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();

  WaitForLaunchParams(new_contents, /*min_launch_params_to_wait_for=*/1);
  EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                  new_contents, "launchParamsTargetUrls"),
              testing::ElementsAre(destination_url));
}

// Ensure that links are captured if Isolated Web App
// was installed without scope extensions and then updated with
// scope extensions.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppLinkCapturingDefaultBehaviorBrowserTest,
                       LinkCaptureAfterUpdateWithScopeExtensions) {
  SetFakeOriginAssociationFetcherData(
      url::Origin::Create(GetCapturableUrlWithQuery()), GetBundleId());
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp(
      ManifestLaunchHandler_ClientMode::kNavigateNew,
      /*with_scope_extensions=*/false, /*version=*/"1.0.0");
  auto bundle_id = url_info.web_bundle_id();

  // Set up the update server and add a new bundle with scope extensions.
  IsolatedWebAppTestUpdateServer update_server;
  url::Origin scope_extension_origin =
      url::Origin::Create(GetCapturableUrlWithQuery());

  auto updated_app =
      IsolatedWebAppBuilder(
          ManifestBuilder()
              .SetName("app-2.0.0")
              .SetVersion("2.0.0")
              .SetLaunchHandlerClientMode(
                  ManifestLaunchHandler_ClientMode::kNavigateNew)
              .AddScopeExtension(scope_extension_origin,
                                 /*has_origin_wildcard=*/false))
          .AddHtml("/", kIwaHtmlContent)
          .AddJs("script.js", kIwaJsContent)
          .BuildBundle(bundle_id,
                       {web_package::test::GetDefaultEd25519KeyPair()});

  updated_app->TrustSigningKey();
  update_server.AddBundle(std::move(updated_app));

  // Force install policy to trigger the update manager.
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::ListValue().Append(
          update_server.CreateForceInstallPolicyEntry(bundle_id)));

  // Trigger the update and wait for it to apply.
  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({url_info.app_id()});

  EXPECT_THAT(provider().isolated_web_app_update_manager().DiscoverUpdatesNow(),
              testing::Eq(1ul));

  manifest_updated_observer.Wait();

  // Verify the app was properly updated to the new version with extensions.
  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_TRUE(web_app);
  EXPECT_EQ("app-2.0.0", web_app->untranslated_name());
  EXPECT_EQ(1UL, web_app->validated_scope_extensions().size());

  // Test that the updated IWA can now properly capture links.
  GURL destination_url = GetCapturableUrlWithQuery();
  CreateLinkInTab(GetBrowserTab(), destination_url, "capture-link-update",
                  /*target=*/"_blank");

  ui_test_utils::BrowserCreatedObserver browser_observer;
  SimulateClickOnElement(GetBrowserTab(), "capture-link-update",
                         blink::WebInputEvent::kNoModifiers,
                         blink::WebMouseEvent::Button::kLeft);

  Browser* new_browser = browser_observer.Wait();
  ASSERT_TRUE(new_browser);

  EXPECT_TRUE(
      AppBrowserController::IsForWebApp(new_browser, url_info.app_id()));
  content::WebContents* new_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();

  // Validate the launch params to ensure full end-to-end payload delivery.
  WaitForLaunchParams(new_contents, /*min_launch_params_to_wait_for=*/1);
  EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                  new_contents, "launchParamsTargetUrls"),
              testing::ElementsAre(destination_url));
}

using IsolatedWebAppLinkCapturingAddValidatedOriginBrowserTest =
    IsolatedWebAppLinkCapturingBrowserTestBase;

IN_PROC_BROWSER_TEST_F(IsolatedWebAppLinkCapturingAddValidatedOriginBrowserTest,
                       TargetBlankClickOpensAppWindow) {
  // Install will not have validated origins.
  InstallIsolatedWebApp(ManifestLaunchHandler_ClientMode::kNavigateNew);

  ASSERT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id(), WebAppFilter::IsIsolatedApp()));
  EXPECT_THAT(
      provider().registrar_unsafe().GetValidatedScopeExtensions(app_id()),
      testing::IsEmpty());

  // Opening IWA should trigger add validated origin associations command.
  // Advance test clock to prevent throttling, because first validation happened
  // during installation of an app.
  clock().Advance(base::Days(2));
  SetFakeOriginAssociationFetcherData(
      url::Origin::Create(GetCapturableUrlWithQuery()), GetBundleId());
  content::RenderFrameHost* frame = OpenIsolatedWebApp(profile(), app_id());
  content::WebContents* existing_app_contents =
      content::WebContents::FromRenderFrameHost(frame);
  BrowserWindowInterface* existing_app_browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          existing_app_contents);
  WaitForLaunchParams(existing_app_contents,
                      /*min_launch_params_to_wait_for=*/1);

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  ASSERT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id(), WebAppFilter::IsIsolatedApp()));
  EXPECT_THAT(
      provider().registrar_unsafe().GetValidatedScopeExtensions(app_id()),
      testing::ElementsAre(ScopeExtensionInfo::CreateForScope(
          GetCapturableUrl(), /*has_origin_wildcard=*/false)));

  // We are already sure that validated scope extensions here, however,
  // ensuring that links are captured is critical to know that
  // this app is default user preference to handle links.

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

}  // namespace web_app
