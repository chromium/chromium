// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/geometry/rect.h"

using content::RenderFrameHost;
using content::WebContents;
using extensions::Extension;

namespace {

constexpr const char kExampleURL[] = "http://example.org/";
constexpr const char kImagePath[] = "/ssl/google_files/logo.gif";
constexpr const char kAppDotComManifest[] =
    "{"
    "  \"name\": \"Hosted App\","
    "  \"version\": \"1\","
    "  \"manifest_version\": 2,"
    "  \"app\": {"
    "    \"launch\": {"
    "      \"web_url\": \"%s\""
    "    },"
    "    \"urls\": [\"*://app.com/\"]"
    "  }"
    "}";

enum class AppType {
  HOSTED_APP,    // Using HostedAppBrowserController
  BOOKMARK_APP,  // Using HostedAppBrowserController
  WEB_APP,       // Using WebAppBrowserController
};

// Opens |url| in a new popup window with the dimensions |popup_size|.
Browser* OpenPopupAndWait(Browser* browser,
                          const GURL& url,
                          const gfx::Size& popup_size) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  content::WebContentsAddedObserver new_contents_observer;
  std::string open_window_script = base::StringPrintf(
      "window.open('%s', '_blank', 'toolbar=none,width=%i,height=%i')",
      url.spec().c_str(), popup_size.width(), popup_size.height());

  EXPECT_TRUE(content::ExecJs(web_contents, open_window_script));

  WebContents* popup_contents = new_contents_observer.GetWebContents();
  content::WaitForLoadStop(popup_contents);
  Browser* popup_browser = chrome::FindBrowserWithWebContents(popup_contents);

  // The navigation should happen in a new window.
  EXPECT_NE(browser, popup_browser);

  return popup_browser;
}

// If |proceed_through_interstitial| is true, asserts that a security
// interstitial is shown, and clicks through it, before returning.
void NavigateToURLAndWait(Browser* browser,
                          const GURL& url,
                          bool proceed_through_interstitial = false) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  {
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
    observer.WaitForNavigationFinished();
  }

  if (!proceed_through_interstitial)
    return;

  {
    // Need a second TestNavigationObserver; the above one is spent.
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            browser->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(
        helper &&
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
    std::string javascript = "window.certificateErrorPageController.proceed();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, javascript));
    observer.Wait();
  }
}

// Used by ShouldShowCustomTabBarForXXX. Performs a navigation and then checks
// that the toolbar visibility is as expected.
void NavigateAndCheckForToolbar(Browser* browser,
                                const GURL& url,
                                bool expected_visibility,
                                bool proceed_through_interstitial = false) {
  NavigateToURLAndWait(browser, url, proceed_through_interstitial);
  EXPECT_EQ(expected_visibility,
            browser->app_controller()->ShouldShowCustomTabBar());
}

void NavigateAndAwaitInstallabilityCheck(Browser* browser, const GURL& url) {
  auto* manager = banners::TestAppBannerManagerDesktop::CreateForWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  NavigateToURLAndWait(browser, url);
  manager->WaitForInstallableCheck();
}

void CheckWebContentsHasAppPrefs(content::WebContents* web_contents) {
  blink::mojom::RendererPreferences* prefs =
      web_contents->GetMutableRendererPrefs();
  EXPECT_FALSE(prefs->can_accept_load_drops);
}

void CheckWebContentsDoesNotHaveAppPrefs(content::WebContents* web_contents) {
  blink::mojom::RendererPreferences* prefs =
      web_contents->GetMutableRendererPrefs();
  EXPECT_TRUE(prefs->can_accept_load_drops);
}

void CheckMixedContentLoaded(Browser* browser) {
  ssl_test_util::CheckSecurityState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ssl_test_util::CertError::NONE, security_state::NONE,
      ssl_test_util::AuthState::DISPLAYED_INSECURE_CONTENT);
}

void CheckMixedContentFailedToLoad(Browser* browser) {
  ssl_test_util::CheckSecurityState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ssl_test_util::CertError::NONE, security_state::SECURE,
      ssl_test_util::AuthState::NONE);
}

// Returns a path string that points to a page with the
// "REPLACE_WITH_HOST_AND_PORT" string replaced with |host_port_pair|.
// The page at |original_path| should contain the string
// "REPLACE_WITH_HOST_AND_PORT".
std::string GetPathWithHostAndPortReplaced(const std::string& original_path,
                                           net::HostPortPair host_port_pair) {
  base::StringPairs replacement_text = {
      {"REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()}};
  return net::test_server::GetFilePathWithReplacements(original_path,
                                                       replacement_text);
}

// Tries to load an image at |image_url| and returns whether or not it loaded
// successfully.
//
// The image could fail to load because it was blocked from being loaded or
// because |image_url| doesn't exist. Therefore, it failing to load is not a
// reliable indicator of insecure content being blocked. Users of the function
// should check the state of security indicators.
bool TryToLoadImage(const content::ToRenderFrameHost& adapter,
                    const GURL& image_url) {
  const std::string script = base::StringPrintf(
      "let i = document.createElement('img');"
      "document.body.appendChild(i);"
      "i.addEventListener('load', () => domAutomationController.send(true));"
      "i.addEventListener('error', () => domAutomationController.send(false));"
      "i.src = '%s';",
      image_url.spec().c_str());

  bool image_loaded;
  CHECK(content::ExecuteScriptAndExtractBool(adapter, script, &image_loaded));
  return image_loaded;
}

bool IsBrowserOpen(const Browser* test_browser) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == test_browser)
      return true;
  }
  return false;
}

enum AppMenuCommandState {
  kEnabled,
  kDisabled,
  kNotPresent,
};

AppMenuCommandState GetAppMenuCommandState(int command_id, Browser* browser) {
  DCHECK(!browser->app_controller())
      << "This check only applies to regular browser windows.";
  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  if (!app_menu_model->GetModelAndIndexForCommandId(command_id, &model,
                                                    &index)) {
    return kNotPresent;
  }
  return model->IsEnabledAt(index) ? kEnabled : kDisabled;
}

}  // namespace

// Parameters are {app_type, desktop_pwa_flag}. |app_type| controls whether it
// is a Hosted or Bookmark app.
class HostedAppTest : public extensions::ExtensionBrowserTest,
                      public ::testing::WithParamInterface<AppType> {
 public:
  HostedAppTest()
      : app_browser_(nullptr),
        app_(nullptr),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (GetParam() == AppType::WEB_APP) {
      scoped_feature_list_.InitWithFeatures(
          {features::kDesktopPWAsUnifiedUiController},
          {predictors::kSpeculativePreconnectFeature});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kDesktopPWAsUnifiedUiController,
               predictors::kSpeculativePreconnectFeature});
    }
  }
  ~HostedAppTest() override {}

  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    app_type_ = GetParam();

    extensions::ExtensionBrowserTest::SetUp();
  }

 protected:
  void SetupAppWithURL(const GURL& app_url) {
    // TODO(ortuno): Use InstallBookmarkApp instead of loading a manifest,
    // if |app_type_ == BOOKMARK_APP|.
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(
        base::StringPrintf(kAppDotComManifest, app_url.spec().c_str()));
    SetupApp(test_app_dir.UnpackedPath());
  }

  void SetupApp(const std::string& app_folder) {
    SetupApp(test_data_dir_.AppendASCII(app_folder));
  }

  void SetupApp(const base::FilePath& app_folder) {
    app_ = InstallExtensionWithSourceAndFlags(
        app_folder, 1, extensions::Manifest::INTERNAL,
        app_type_ == AppType::HOSTED_APP
            ? extensions::Extension::NO_FLAGS
            : extensions::Extension::FROM_BOOKMARK);
    ASSERT_TRUE(app_);

    LaunchApp();
  }

  void LaunchApp() {
    // Launch app in a window.
    app_browser_ = LaunchAppBrowser(app_);
    ASSERT_TRUE(app_browser_);
    ASSERT_TRUE(app_browser_ != browser());
  }

  GURL GetMixedContentAppURL() {
    return https_server()->GetURL("app.com",
                                  "/ssl/page_displays_insecure_content.html");
  }

  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetInstallableAppURL() {
    return https_server()->GetURL("/banners/manifest_test_page.html");
  }

  static const char* GetInstallableAppName() { return "Manifest test app"; }

  GURL GetURLForPath(std::string path) {
    return https_server_.GetURL("app.com", path);
  }

  GURL GetSecureIFrameAppURL() {
    net::HostPortPair host_port_pair = net::HostPortPair::FromURL(
        https_server()->GetURL("foo.com", "/simple.html"));
    const std::string path = GetPathWithHostAndPortReplaced(
        "/ssl/page_with_cross_site_frame.html", host_port_pair);

    return https_server_.GetURL("app.com", path);
  }

  void InstallMixedContentPWA() { return InstallPWA(GetMixedContentAppURL()); }

  void InstallMixedContentIFramePWA() {
    net::HostPortPair host_port_pair = net::HostPortPair::FromURL(
        https_server()->GetURL("foo.com", "/simple.html"));
    const std::string path = GetPathWithHostAndPortReplaced(
        "/ssl/page_displays_insecure_content_in_iframe.html", host_port_pair);

    InstallPWA(https_server()->GetURL("app.com", path));
  }

  void InstallSecurePWA() { return InstallPWA(GetSecureAppURL()); }

  void InstallSecureIFramePWA() { return InstallPWA(GetSecureIFrameAppURL()); }

  void InstallPWA(const GURL& app_url) {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = app_url;
    web_app_info.scope = app_url.GetWithoutFilename();
    web_app_info.open_as_window = true;

    app_ = InstallBookmarkApp(web_app_info);

    ui_test_utils::UrlLoadObserver url_observer(
        app_url, content::NotificationService::AllSources());
    app_browser_ = LaunchAppBrowser(app_);
    url_observer.Wait();

    CHECK(app_browser_);
    CHECK(app_browser_ != browser());
  }

  web_app::AppId InstallPwaForCurrentUrl() {
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
    web_app::WebAppInstallObserver observer(profile());
    CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
    web_app::AppId app_id = observer.AwaitNextInstall();
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
    return app_id;
  }

  web_app::AppId InstallShortcutAppForCurrentUrl() {
    chrome::SetAutoAcceptBookmarkAppDialogForTesting(true);
    web_app::WebAppInstallObserver observer(profile());
    CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
    web_app::AppId app_id = observer.AwaitNextInstall();
    chrome::SetAutoAcceptBookmarkAppDialogForTesting(false);
    return app_id;
  }

  Browser* NavigateInNewWindowAndAwaitInstallabilityCheck(const GURL& url) {
    Browser* new_browser = new Browser(
        Browser::CreateParams(Browser::TYPE_NORMAL, profile(), true));
    AddBlankTabAndShow(new_browser);
    NavigateAndAwaitInstallabilityCheck(new_browser, url);
    return new_browser;
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
    cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // By default, all SSL cert checks are valid. Can be overridden in tests.
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  // Tests that performing |action| results in a new foreground tab
  // that navigated to |target_url| in the main browser window.
  void TestAppActionOpensForegroundTab(base::OnceClosure action,
                                       const GURL& target_url) {
    ASSERT_EQ(app_browser_, chrome::FindLastActive());

    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    ASSERT_NO_FATAL_FAILURE(std::move(action).Run());

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());

    content::WebContents* new_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_NE(initial_tab, new_tab);
    EXPECT_EQ(target_url, new_tab->GetLastCommittedURL());
  }

  web_app::AppRegistrar& registrar() {
    auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());
    CHECK(provider);
    return provider->registrar();
  }

  web_app::AppRegistryController& registry_controller() {
    auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());
    CHECK(provider);
    return provider->registry_controller();
  }

  Browser* app_browser_;
  const extensions::Extension* app_;

  AppType app_type() const { return app_type_; }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::ContentMockCertVerifier::CertVerifier* cert_verifier() {
    return cert_verifier_.mock_cert_verifier();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  AppType app_type_;

  net::EmbeddedTestServer https_server_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppTest);
};

using SharedAppTest = HostedAppTest;

// Tests that "Open link in new tab" opens a link in a foreground tab.
IN_PROC_BROWSER_TEST_P(HostedAppTest, OpenLinkInNewTab) {
  SetupApp("app");

  const GURL url("http://www.foo.com/");
  TestAppActionOpensForegroundTab(
      base::BindOnce(
          [](content::WebContents* app_contents, const GURL& target_url) {
            ui_test_utils::UrlLoadObserver url_observer(
                target_url, content::NotificationService::AllSources());
            content::ContextMenuParams params;
            params.page_url = app_contents->GetLastCommittedURL();
            params.link_url = target_url;

            TestRenderViewContextMenu menu(app_contents->GetMainFrame(),
                                           params);
            menu.Init();
            menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                0 /* event_flags */);
            url_observer.Wait();
          },
          app_browser_->tab_strip_model()->GetActiveWebContents(), url),
      url);
}

// Tests that Ctrl + Clicking a link opens a foreground tab.
IN_PROC_BROWSER_TEST_P(HostedAppTest, CtrlClickLink) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up an app which covers app.com URLs.
  GURL app_url =
      embedded_test_server()->GetURL("app.com", "/click_modifier/href.html");
  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kAppDotComManifest, app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());
  // Wait for the URL to load so that we can click on the page.
  url_observer.Wait();

  const GURL url = embedded_test_server()->GetURL(
      "app.com", "/click_modifier/new_window.html");
  TestAppActionOpensForegroundTab(
      base::BindOnce(
          [](content::WebContents* app_contents, const GURL& target_url) {
            ui_test_utils::UrlLoadObserver url_observer(
                target_url, content::NotificationService::AllSources());
            int ctrl_key;
#if defined(OS_MACOSX)
            ctrl_key = blink::WebInputEvent::Modifiers::kMetaKey;
#else
            ctrl_key = blink::WebInputEvent::Modifiers::kControlKey;
#endif
            content::SimulateMouseClick(app_contents, ctrl_key,
                                        blink::WebMouseEvent::Button::kLeft);
            url_observer.Wait();
          },
          app_browser_->tab_strip_model()->GetActiveWebContents(), url),
      url);
}

// Tests that the WebContents of an app window launched using OpenApplication
// has the correct prefs.
IN_PROC_BROWSER_TEST_P(HostedAppTest, WebContentsPrefsOpenApplication) {
  SetupApp("https_app");
  CheckWebContentsHasAppPrefs(
      app_browser_->tab_strip_model()->GetActiveWebContents());
}

// Tests that the WebContents of an app window launched using
// web_app::ReparentWebContentsIntoAppBrowser has the correct prefs.
IN_PROC_BROWSER_TEST_P(HostedAppTest, WebContentsPrefsReparentWebContents) {
  SetupApp("https_app");

  content::WebContents* current_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  CheckWebContentsDoesNotHaveAppPrefs(current_tab);

  Browser* app_browser =
      web_app::ReparentWebContentsIntoAppBrowser(current_tab, app_->id());
  ASSERT_NE(browser(), app_browser);

  CheckWebContentsHasAppPrefs(
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents());
}

// Tests that the WebContents of a regular browser window launched using
// OpenInChrome has the correct prefs.
IN_PROC_BROWSER_TEST_P(HostedAppTest, WebContentsPrefsOpenInChrome) {
  SetupApp("https_app");

  content::WebContents* app_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  CheckWebContentsHasAppPrefs(app_contents);

  chrome::OpenInChrome(app_browser_);
  ASSERT_EQ(browser(), chrome::FindLastActive());

  CheckWebContentsDoesNotHaveAppPrefs(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Check that the toolbar is shown correctly.
IN_PROC_BROWSER_TEST_P(HostedAppTest, ShouldShowCustomTabBar) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = https_server()->GetURL("app.com", "/simple.html");

  SetupAppWithURL(app_url);

  // Navigate to the app's launch page; the toolbar should be hidden.
  NavigateAndCheckForToolbar(app_browser_, app_url, false);

  // Navigate to another page on the same origin; the toolbar should still
  // hidden.
  NavigateAndCheckForToolbar(
      app_browser_, https_server()->GetURL("app.com", "/empty.html"), false);

  // Navigate to different origin; the toolbar should now be visible.
  NavigateAndCheckForToolbar(
      app_browser_, https_server()->GetURL("foo.com", "/simple.html"), true);
}

class HostedAppTestWithAutoupgradesDisabled : public HostedAppTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HostedAppTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_P(HostedAppTestWithAutoupgradesDisabled,
                       ShouldShowCustomTabBarMixedContent) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = https_server()->GetURL("app.com", "/");

  SetupAppWithURL(app_url);

  // Navigate to another page on the same origin, but with mixed content; the
  // toolbar should be shown.
  NavigateAndCheckForToolbar(
      app_browser_,
      https_server()->GetURL("app.com",
                             "/ssl/page_displays_insecure_content.html"),
      true);
}

IN_PROC_BROWSER_TEST_P(HostedAppTestWithAutoupgradesDisabled,
                       ShouldShowCustomTabBarDynamicMixedContent) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = https_server()->GetURL("app.com", "/simple.html");

  SetupAppWithURL(app_url);

  // Navigate to a page on the same origin. Since mixed content hasn't been
  // loaded yet, the toolbar shouldn't be shown.
  NavigateAndCheckForToolbar(app_browser_, app_url, false);

  // Load mixed content; now the toolbar should be shown.
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(TryToLoadImage(
      web_contents, embedded_test_server()->GetURL("foo.com", kImagePath)));
  EXPECT_TRUE(app_browser_->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(HostedAppTest,
                       ShouldShowCustomTabBarForHTTPAppSameOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url =
      embedded_test_server()->GetURL("app.com", "/simple.html");
  SetupAppWithURL(app_url);

  // Navigate to the app's launch page; the toolbar should be visible, even
  // though it exactly matches the site, because it is not secure.
  NavigateAndCheckForToolbar(app_browser_, app_url, true);
}

IN_PROC_BROWSER_TEST_P(HostedAppTest,
                       ShouldShowCustomTabBarForHTTPAppHTTPSUrl) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = https_server()->GetURL("app.com", "/simple.html");

  GURL::Replacements scheme_http;
  scheme_http.SetSchemeStr("http");

  // Create an app that has the same port and origin as `app_url` but with a
  // "http" scheme.
  SetupAppWithURL(app_url.ReplaceComponents(scheme_http));

  // Navigate to the https version of the site; the toolbar should
  // be hidden, as it is a more secure version of the site.
  NavigateAndCheckForToolbar(app_browser_, app_url, false);
}

IN_PROC_BROWSER_TEST_P(HostedAppTest,
                       ShouldShowCustomTabBarForHTTPSAppSameOrigin) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = https_server()->GetURL("app.com", "/simple.html");
  SetupAppWithURL(app_url);

  // Navigate to the app's launch page; the toolbar should be hidden.
  NavigateAndCheckForToolbar(app_browser_, app_url, false);
}

// Check that the toolbar is shown correctly for HTTPS apps when they
// navigate to a HTTP page on the same origin.
IN_PROC_BROWSER_TEST_P(HostedAppTest,
                       ShouldShowCustomTabBarForHTTPSAppHTTPUrl) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = https_server()->GetURL("app.com", "/simple.html");
  SetupAppWithURL(app_url);

  GURL::Replacements scheme_http;
  scheme_http.SetSchemeStr("http");

  // Navigate to the http version of the site; the toolbar should
  // be visible for the https version as it is not secure.
  NavigateAndCheckForToolbar(app_browser_,
                             app_url.ReplaceComponents(scheme_http), true);
}

// Check that the toolbar is shown correctly for apps that specify start
// URLs without the 'www.' prefix.
IN_PROC_BROWSER_TEST_P(HostedAppTest, ShouldShowCustomTabBarForAppWithoutWWW) {
  ASSERT_TRUE(https_server()->Start());

  const GURL app_url = https_server()->GetURL("app.com", "/simple.html");
  SetupAppWithURL(app_url);

  // Navigate to the app's launch page; the toolbar should be hidden.
  NavigateAndCheckForToolbar(app_browser_, app_url, false);

  // Navigate to the app's launch page with the 'www.' prefix; the toolbar
  // should be hidden.
  NavigateAndCheckForToolbar(
      app_browser_, https_server()->GetURL("www.app.com", "/simple.html"),
      false);

  // Navigate to different origin; the toolbar should now be visible.
  NavigateAndCheckForToolbar(
      app_browser_, https_server()->GetURL("www.foo.com", "/simple.html"),
      true);
}

// Check that a subframe on a regular web page can navigate to a URL that
// redirects to a hosted app.  https://crbug.com/721949.
IN_PROC_BROWSER_TEST_P(SharedAppTest, SubframeRedirectsToHostedApp) {
  // This test only applies to hosted apps.
  if (app_type() != AppType::HOSTED_APP)
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up an app which covers app.com URLs.
  GURL app_url = embedded_test_server()->GetURL("app.com", "/title1.html");
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kAppDotComManifest, app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());

  // Navigate a regular tab to a page with a subframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/iframe.html");
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateToURLAndWait(browser(), url);

  // Navigate the subframe to a URL that redirects to a URL in the hosted app's
  // web extent.
  GURL redirect_url = embedded_test_server()->GetURL(
      "bar.com", "/server-redirect?" + app_url.spec());
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", redirect_url));

  // Ensure that the frame navigated successfully and that it has correct
  // content.
  RenderFrameHost* subframe = content::ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_EQ(app_url, subframe->GetLastCommittedURL());
  EXPECT_EQ(
      "This page has no title.",
      EvalJs(subframe, "document.body.innerText.trim();").ExtractString());
}

// Check that no assertions are hit when showing a permission request bubble.
IN_PROC_BROWSER_TEST_P(HostedAppTest, PermissionBubble) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebApplicationInfo web_app_info;
  web_app_info.app_url = GetSecureAppURL();
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);

  ui_test_utils::UrlLoadObserver url_observer(
      GetSecureAppURL(), content::NotificationService::AllSources());
  Browser* app_browser = LaunchAppBrowser(app);
  url_observer.Wait();

  RenderFrameHost* render_frame_host =
      app_browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  EXPECT_TRUE(content::ExecuteScript(
      render_frame_host,
      "navigator.geolocation.getCurrentPosition(function(){});"));
}

// Tests that regular Hosted Apps and Bookmark Apps can still load mixed
// content.
IN_PROC_BROWSER_TEST_P(HostedAppTestWithAutoupgradesDisabled,
                       MixedContentInBookmarkApp) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetMixedContentAppURL();

  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kAppDotComManifest, app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());
  url_observer.Wait();

  CheckMixedContentLoaded(app_browser_);
}

// Ensure that hosted app windows with blank titles don't display the URL as a
// default window title.
IN_PROC_BROWSER_TEST_P(HostedAppTest, EmptyTitlesDoNotDisplayUrl) {
  ASSERT_TRUE(https_server()->Start());
  GURL url = https_server()->GetURL("app.site.com", "/empty.html");

  WebApplicationInfo web_app_info;
  web_app_info.app_url = url;
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);

  Browser* app_browser = LaunchAppBrowser(app);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(base::string16(), app_browser->GetWindowTitleForCurrentTab(false));
  NavigateToURLAndWait(app_browser,
                       https_server()->GetURL("app.site.com", "/simple.html"));
  EXPECT_EQ(base::ASCIIToUTF16("OK"),
            app_browser->GetWindowTitleForCurrentTab(false));
}

// Ensure that hosted app windows display the app title instead of the page
// title when off scope.
IN_PROC_BROWSER_TEST_P(HostedAppTest, OffScopeUrlsDisplayAppTitle) {
  ASSERT_TRUE(https_server()->Start());
  GURL url = GetSecureAppURL();

  WebApplicationInfo web_app_info;
  web_app_info.app_url = url;
  web_app_info.scope = url.GetWithoutFilename();
  web_app_info.title = base::ASCIIToUTF16("A Hosted App");
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);

  Browser* app_browser = LaunchAppBrowser(app);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);

  // When we are within scope, show the page title.
  EXPECT_EQ(base::ASCIIToUTF16("Google"),
            app_browser->GetWindowTitleForCurrentTab(false));

  NavigateToURLAndWait(app_browser,
                       https_server()->GetURL("app.site.com", "/simple.html"));

  // When we are off scope, show the app title.
  EXPECT_EQ(base::ASCIIToUTF16("A Hosted App"),
            app_browser->GetWindowTitleForCurrentTab(false));
}

// Ensure that hosted app windows display the app title instead of the page
// title when using http.
IN_PROC_BROWSER_TEST_P(HostedAppTest, InScopeHttpUrlsDisplayAppTitle) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("app.site.com", "/simple.html");
  WebApplicationInfo web_app_info;
  web_app_info.app_url = url;
  web_app_info.title = base::ASCIIToUTF16("A Hosted App");

  const extensions::Extension* app = InstallBookmarkApp(web_app_info);

  Browser* app_browser = LaunchAppBrowser(app);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);

  // The page title is "OK" but the page is being served over HTTP, so the app
  // title should be used instead.
  EXPECT_EQ(base::ASCIIToUTF16("A Hosted App"),
            app_browser->GetWindowTitleForCurrentTab(false));
}

using HostedAppPWAOnlyTest = HostedAppTest;
using SharedPWATest = HostedAppTest;
using HostedAppPWAOnlyTestWithAutoupgradesDisabled =
    HostedAppTestWithAutoupgradesDisabled;

// Tests that the command for popping a tab out to a PWA window is disabled in
// incognito.
IN_PROC_BROWSER_TEST_P(SharedPWATest, PopOutDisabledInIncognito) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecurePWA();

  Browser* incognito_browser =
      OpenURLOffTheRecord(profile(), GetSecureAppURL());
  auto app_menu_model =
      std::make_unique<AppMenuModel>(nullptr, incognito_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  ASSERT_TRUE(app_menu_model->GetModelAndIndexForCommandId(
      IDC_OPEN_IN_PWA_WINDOW, &model, &index));
  EXPECT_FALSE(model->IsEnabledAt(index));
}

// Tests that desktop PWAs open links in the browser.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, DesktopPWAsOpenLinksInApp) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecurePWA();
  ASSERT_TRUE(
      extensions::util::GetInstalledPwaForUrl(profile(), GetSecureAppURL()));

  NavigateToURLAndWait(app_browser_, GetSecureAppURL());

  ASSERT_TRUE(app_browser_->app_controller());

  NavigateAndCheckForToolbar(app_browser_, GURL(kExampleURL), true);
}

// Tests that desktop PWAs are opened at the correct size.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, PWASizeIsCorrectlyRestored) {
  ASSERT_TRUE(https_server()->Start());

  InstallSecurePWA();

  EXPECT_TRUE(web_app::AppBrowserController::IsForWebAppBrowser(app_browser_));
  NavigateToURLAndWait(app_browser_, GetSecureAppURL());

  gfx::Rect bounds = gfx::Rect(10, 10, 500, 500);
  app_browser_->window()->SetBounds(bounds);
  app_browser_->window()->Close();

  Browser* new_browser = LaunchAppBrowser(app_);
  EXPECT_EQ(new_browser->window()->GetBounds(), bounds);
}

// Tests that desktop PWAs are reopened at the correct size.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest,
                       ReopenedPWASizeIsCorrectlyRestored) {
  ASSERT_TRUE(https_server()->Start());

  InstallSecurePWA();

  EXPECT_TRUE(web_app::AppBrowserController::IsForWebAppBrowser(app_browser_));
  NavigateToURLAndWait(app_browser_, GetSecureAppURL());

  gfx::Rect bounds = gfx::Rect(10, 10, 500, 500);
  app_browser_->window()->SetBounds(bounds);
  app_browser_->window()->Close();

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* restored_browser =
      chrome::FindBrowserWithWebContents(restored_web_contents);
  EXPECT_EQ(restored_browser->window()->GetBounds(), bounds);
}

// Tests that using window.open to create a popup window out of scope results in
// a correctly sized window.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, OffScopePWAPopupsHaveCorrectSize) {
  ASSERT_TRUE(https_server()->Start());

  InstallSecurePWA();

  LaunchApp();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebAppBrowser(app_browser_));

  GURL offscope_url("https://example.com");
  gfx::Size size(500, 500);

  Browser* popup_browser = OpenPopupAndWait(app_browser_, offscope_url, size);

  // The navigation should have happened in a new window.
  EXPECT_NE(popup_browser, app_browser_);

  // The popup browser should be a PWA.
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebAppBrowser(popup_browser));

  // Toolbar should be shown, as the popup is out of scope.
  EXPECT_TRUE(popup_browser->app_controller()->ShouldShowCustomTabBar());

  // Skip animating the toolbar visibility.
  popup_browser->app_controller()->UpdateCustomTabBarVisibility(false);

  // The popup window should be the size we specified.
  EXPECT_EQ(size, popup_browser->window()->GetContentsSize());
}

// Tests that using window.open to create a popup window in scope results in
// a correctly sized window.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, InScopePWAPopupsHaveCorrectSize) {
  ASSERT_TRUE(https_server()->Start());

  InstallSecurePWA();

  LaunchApp();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebAppBrowser(app_browser_));

  gfx::Size size(500, 500);
  Browser* popup_browser =
      OpenPopupAndWait(app_browser_, GetSecureAppURL(), size);

  // The navigation should have happened in a new window.
  EXPECT_NE(popup_browser, app_browser_);

  // The popup browser should be a PWA.
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebAppBrowser(popup_browser));

  // Toolbar should not be shown, as the popup is in scope.
  EXPECT_FALSE(popup_browser->app_controller()->ShouldShowCustomTabBar());

  // Skip animating the toolbar visibility.
  popup_browser->app_controller()->UpdateCustomTabBarVisibility(false);

  // The popup window should be the size we specified.
  EXPECT_EQ(size, popup_browser->window()->GetContentsSize());
}

// Tests that app windows are correctly restored.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, RestoreAppWindow) {
  ASSERT_TRUE(https_server()->Start());

  InstallSecurePWA();
  ASSERT_TRUE(app_browser_->is_type_app());
  app_browser_->window()->Close();

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* restored_browser =
      chrome::FindBrowserWithWebContents(restored_web_contents);

  EXPECT_TRUE(restored_browser->is_type_app());
}

// Tests that app windows are restored in a tab if the app is uninstalled.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest,
                       RestoreAppWindowForUninstalledApp) {
  ASSERT_TRUE(https_server()->Start());

  InstallSecurePWA();
  ASSERT_TRUE(app_browser_->is_type_app());
  ASSERT_FALSE(app_browser_->is_type_normal());
  app_browser_->window()->Close();

  extensions::TestExtensionRegistryObserver test_observer(
      extensions::ExtensionRegistry::Get(browser()->profile()), app_->id());
  UninstallExtension(app_->id());
  test_observer.WaitForExtensionUninstalled();

  content::WebContentsAddedObserver new_contents_observer;

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile());
  service->RestoreMostRecentEntry(nullptr);

  content::WebContents* restored_web_contents =
      new_contents_observer.GetWebContents();
  Browser* restored_browser =
      chrome::FindBrowserWithWebContents(restored_web_contents);

  EXPECT_FALSE(restored_browser->is_type_app());
  EXPECT_TRUE(restored_browser->is_type_normal());
}

// Test navigating to an out of scope url on the same origin causes the url
// to be shown to the user.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest,
                       LocationBarIsVisibleOffScopeOnSameOrigin) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecurePWA();

  // Toolbar should not be visible in the app.
  ASSERT_FALSE(app_browser_->app_controller()->ShouldShowCustomTabBar());

  // The installed PWA's scope is app.com:{PORT}/ssl,
  // so app.com:{PORT}/accessibility_fail.html is out of scope.
  const GURL& out_of_scope = GetURLForPath("/accessibility_fail.html");

  NavigateToURLAndWait(app_browser_, out_of_scope);

  // Location should be visible off scope.
  ASSERT_TRUE(app_browser_->app_controller()->ShouldShowCustomTabBar());
}

// Tests that PWA menus have an uninstall option.
IN_PROC_BROWSER_TEST_P(SharedPWATest, UninstallMenuOption) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecurePWA();

  auto app_menu_model =
      std::make_unique<WebAppMenuModel>(nullptr, app_browser_);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  bool found = app_menu_model->GetModelAndIndexForCommandId(
      WebAppMenuModel::kUninstallAppCommandId, &model, &index);
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(found);
#else
  EXPECT_TRUE(found);
  EXPECT_TRUE(model->IsEnabledAt(index));
#endif  // defined(OS_CHROMEOS)
}

// Tests that both installing a PWA and creating a shortcut app are disabled for
// incognito windows.
IN_PROC_BROWSER_TEST_P(SharedPWATest, ShortcutMenuOptionsInIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  auto* manager = banners::TestAppBannerManagerDesktop::CreateForWebContents(
      incognito_browser->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(incognito_browser, GetSecureAppURL());
  EXPECT_FALSE(manager->WaitForInstallableCheck());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, incognito_browser),
            kDisabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, incognito_browser),
            kNotPresent);
}

// Tests that both installing a PWA and creating a shortcut app are available
// for an installable PWA.
IN_PROC_BROWSER_TEST_P(SharedPWATest, ShortcutMenuOptionsForInstallablePWA) {
  auto* manager = banners::TestAppBannerManagerDesktop::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  EXPECT_TRUE(manager->WaitForInstallableCheck());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kEnabled);
}

// Tests that creating a shortcut app but not installing a PWA is available for
// a non-installable site.
IN_PROC_BROWSER_TEST_P(SharedPWATest,
                       ShortcutMenuOptionsForNonInstallableSite) {
  auto* manager = banners::TestAppBannerManagerDesktop::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetMixedContentAppURL());
  EXPECT_FALSE(manager->WaitForInstallableCheck());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kNotPresent);
}

// Tests that an installed PWA is not used when out of scope by one path level.
IN_PROC_BROWSER_TEST_P(SharedPWATest, MenuOptionsOutsideInstalledPwaScope) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(
      browser(),
      https_server()->GetURL("/banners/scope_is_start_url/index.html"));
  InstallPwaForCurrentUrl();

  // Open a page that is one directory up from the installed PWA.
  Browser* new_browser = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL("/banners/no_manifest_test_page.html"));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_P(SharedPWATest, InstallInstallableSite) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  web_app::AppId app_id = InstallPwaForCurrentUrl();
  EXPECT_EQ(registrar().GetAppShortName(app_id), GetInstallableAppName());

  // Installed PWAs should launch in their own window.
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            blink::mojom::DisplayMode::kStandalone);

  EXPECT_EQ(1, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("CreateShortcut"));
}

IN_PROC_BROWSER_TEST_P(SharedPWATest, CreateShortcutForInstallableSite) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  web_app::AppId app_id = InstallShortcutAppForCurrentUrl();
  EXPECT_EQ(registrar().GetAppShortName(app_id), GetInstallableAppName());
  // Bookmark apps to PWAs should launch in a tab.
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            blink::mojom::DisplayMode::kBrowser);

  EXPECT_EQ(0, user_action_tester.GetActionCount("InstallWebAppFromMenu"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("CreateShortcut"));
}

IN_PROC_BROWSER_TEST_P(SharedPWATest, CanInstallOverTabShortcutApp) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  InstallShortcutAppForCurrentUrl();

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_P(SharedPWATest, CanInstallOverTabPwa) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  web_app::AppId app_id = InstallPwaForCurrentUrl();
  // Change display mode to open in tab.
  registry_controller().SetAppUserDisplayMode(
      app_id, blink::mojom::DisplayMode::kBrowser);

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kNotPresent);
}

IN_PROC_BROWSER_TEST_P(SharedPWATest, CannotInstallOverWindowShortcutApp) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  web_app::AppId app_id = InstallShortcutAppForCurrentUrl();
  // Change launch container to open in window.
  registry_controller().SetAppUserDisplayMode(
      app_id, blink::mojom::DisplayMode::kStandalone);

  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

IN_PROC_BROWSER_TEST_P(SharedPWATest, CannotInstallOverWindowPwa) {
  ASSERT_TRUE(https_server()->Start());

  NavigateToURLAndWait(browser(), GetInstallableAppURL());
  InstallPwaForCurrentUrl();

  // Avoid any interference if active browser was changed by PWA install.
  Browser* new_browser =
      NavigateInNewWindowAndAwaitInstallabilityCheck(GetInstallableAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, new_browser), kEnabled);
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, new_browser), kNotPresent);
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, new_browser),
            kEnabled);
}

// Tests that the command for OpenActiveTabInPwaWindow is available for secure
// pages in an app's scope.
IN_PROC_BROWSER_TEST_P(SharedPWATest, ReparentWebAppForSecureActiveTab) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecurePWA();

  NavigateToURLAndWait(browser(), GetSecureAppURL());
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), GetSecureAppURL());

  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);

  Browser* app_browser = web_app::ReparentWebAppForSecureActiveTab(browser());

  ASSERT_EQ(app_browser->app_controller()->GetAppId(), app_->id());
}

// Tests that reparenting the last browser tab doesn't close the browser window.
IN_PROC_BROWSER_TEST_P(SharedPWATest, ReparentLastBrowserTab) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecurePWA();
  NavigateToURLAndWait(browser(), GetSecureAppURL());

  Browser* app_browser = web_app::ReparentWebAppForSecureActiveTab(browser());
  ASSERT_EQ(app_browser->app_controller()->GetAppId(), app_->id());

  ASSERT_TRUE(IsBrowserOpen(browser()));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

// Tests that the manifest name of the current installable site is used in the
// installation menu text.
IN_PROC_BROWSER_TEST_P(SharedPWATest, InstallToShelfContainsAppName) {
  auto* manager = banners::TestAppBannerManagerDesktop::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(https_server()->Start());
  NavigateToURLAndWait(browser(), GetInstallableAppURL());

  EXPECT_TRUE(manager->WaitForInstallableCheck());

  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser());
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  EXPECT_TRUE(app_menu_model->GetModelAndIndexForCommandId(IDC_INSTALL_PWA,
                                                           &model, &index));
  EXPECT_EQ(app_menu_model.get(), model);
  EXPECT_EQ(model->GetLabelAt(index),
            base::UTF8ToUTF16("Install Manifest test app\xE2\x80\xA6"));
}

IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, OverscrollEnabled) {
  ASSERT_TRUE(https_server()->Start());
  InstallSecurePWA();
  EXPECT_TRUE(app_browser_->CanOverscrollContent());
}

// Tests that mixed content is not loaded inside PWA windows.
IN_PROC_BROWSER_TEST_P(SharedPWATest, MixedContentInPWA) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallMixedContentPWA();
  CheckMixedContentFailedToLoad(app_browser_);
}

// Tests that when calling OpenInChrome, mixed content can be loaded in the new
// tab.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTestWithAutoupgradesDisabled,
                       MixedContentOpenInChrome) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallMixedContentPWA();

  // Mixed content is not allowed in PWAs.
  CheckMixedContentFailedToLoad(app_browser_);

  chrome::OpenInChrome(app_browser_);
  ASSERT_EQ(browser(), chrome::FindLastActive());
  ASSERT_EQ(GetMixedContentAppURL(), browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetLastCommittedURL());

  // The WebContents is just reparented, so mixed content is still not loaded.
  CheckMixedContentFailedToLoad(browser());
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);

  ui_test_utils::UrlLoadObserver url_observer(
      GetMixedContentAppURL(), content::NotificationService::AllSources());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  url_observer.Wait();

  // After reloading, mixed content should successfully load because the
  // WebContents is no longer in a PWA window.

  CheckMixedContentLoaded(browser());
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kNotPresent);
  EXPECT_EQ(web_app::ReparentWebAppForSecureActiveTab(browser()), nullptr);
}

// Tests that when calling web_app::ReparentWebContentsIntoAppBrowser, mixed
// content cannot be loaded in the new app window.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTestWithAutoupgradesDisabled,
                       MixedContentReparentWebContentsIntoAppBrowser) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallMixedContentPWA();

  NavigateToURLAndWait(browser(), GetMixedContentAppURL());
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), GetMixedContentAppURL());

  // A regular tab should be able to load mixed content.
  CheckMixedContentLoaded(browser());
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kNotPresent);

  Browser* app_browser =
      web_app::ReparentWebContentsIntoAppBrowser(tab_contents, app_->id());

  ASSERT_NE(app_browser, browser());
  ASSERT_EQ(GetMixedContentAppURL(), app_browser->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetLastCommittedURL());

  // After reparenting, the WebContents should still have its mixed content
  // loaded. Note that in practice, this should never happen for PWAs. Users
  // won't be able to reparent WebContents if there is mixed content loaded
  // in them.
  CheckMixedContentLoaded(app_browser);

  ui_test_utils::UrlLoadObserver url_observer(
      GetMixedContentAppURL(), content::NotificationService::AllSources());
  chrome::Reload(app_browser, WindowOpenDisposition::CURRENT_TAB);
  url_observer.Wait();

  // After reloading, mixed content should fail to load, because the WebContents
  // is now in a PWA window.
  CheckMixedContentFailedToLoad(app_browser);
}

// Tests that mixed content is not loaded inside iframes in PWA windows.
IN_PROC_BROWSER_TEST_P(SharedPWATest, IFrameMixedContentInPWA) {
  ASSERT_TRUE(https_server()->Start());

  InstallMixedContentIFramePWA();

  CheckMixedContentFailedToLoad(app_browser_);
}

// Tests that iframes can't dynamically load mixed content in a PWA window, when
// the iframe was created in a regular tab.
IN_PROC_BROWSER_TEST_P(
    HostedAppPWAOnlyTestWithAutoupgradesDisabled,
    IFrameDynamicMixedContentInPWAReparentWebContentsIntoAppBrowser) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecureIFramePWA();

  NavigateToURLAndWait(browser(), GetSecureIFrameAppURL());
  CheckMixedContentFailedToLoad(browser());

  app_browser_ = web_app::ReparentWebContentsIntoAppBrowser(
      browser()->tab_strip_model()->GetActiveWebContents(), app_->id());
  CheckMixedContentFailedToLoad(app_browser_);

  content::RenderFrameHost* main_frame =
      app_browser_->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);
  EXPECT_FALSE(TryToLoadImage(
      iframe, embedded_test_server()->GetURL("foo.com", kImagePath)));

  CheckMixedContentFailedToLoad(app_browser_);
}

// Tests that iframes can dynamically load mixed content in a regular browser
// tab, when the iframe was created in a PWA window.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTestWithAutoupgradesDisabled,
                       IFrameDynamicMixedContentInPWAOpenInChrome) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecureIFramePWA();

  chrome::OpenInChrome(app_browser_);

  content::RenderFrameHost* main_frame =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);

  EXPECT_TRUE(TryToLoadImage(
      iframe, embedded_test_server()->GetURL("foo.com", kImagePath)));

  CheckMixedContentLoaded(browser());
}

// Check that uninstalling a PWA with a window opened doesn't crash.
IN_PROC_BROWSER_TEST_P(SharedPWATest, UninstallPwaWithWindowOpened) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());
  InstallSecurePWA();

  EXPECT_TRUE(IsBrowserOpen(app_browser_));

  // TODO (crbug.com/876576): Remove references to extensions in SharedPWATest.
  UninstallExtension(app_->id());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsBrowserOpen(app_browser_));
}

// PWAs moved to tabbed browsers should not get closed when uninstalled.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, UninstallPwaWithWindowMovedToTab) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallSecurePWA();

  EXPECT_TRUE(IsBrowserOpen(app_browser_));

  Browser* tabbed_browser = chrome::OpenInChrome(app_browser_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBrowserOpen(tabbed_browser));
  EXPECT_EQ(tabbed_browser, browser());
  EXPECT_FALSE(IsBrowserOpen(app_browser_));

  UninstallExtension(app_->id());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBrowserOpen(tabbed_browser));
  EXPECT_EQ(tabbed_browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            GetSecureAppURL());
}

IN_PROC_BROWSER_TEST_P(HostedAppTest, CreatedForInstalledPwaForNonPwas) {
  SetupApp("https_app");

  EXPECT_FALSE(app_browser_->app_controller()->CreatedForInstalledPwa());
}

// Check the 'Copy URL' menu button for Hosted App windows.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, CopyURL) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kExampleURL);
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);
  Browser* app_browser = LaunchAppBrowser(app);

  content::BrowserTestClipboardScope test_clipboard_scope;
  chrome::ExecuteCommand(app_browser, IDC_COPY_URL);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  base::string16 result;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &result);
  EXPECT_EQ(result, base::UTF8ToUTF16(kExampleURL));
}

// Check the 'Open in Chrome' menu button for Hosted App windows.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, OpenInChrome) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kExampleURL);
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);
  {
    Browser* app_browser = LaunchAppBrowser(app);

    EXPECT_EQ(1, app_browser->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

    chrome::ExecuteCommand(app_browser, IDC_OPEN_IN_CHROME);

    // The browser frame is closed next event loop so it's still safe to access
    // here.
    EXPECT_EQ(0, app_browser->tab_strip_model()->count());

    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
    EXPECT_EQ(
        GURL(kExampleURL),
        browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  }

  // Wait until the browser actually gets closed. This invalidates
  // |app_browser|.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

// This feature is only available for ChromeOS at the moment.
#if defined(OS_CHROMEOS)
// Check the 'App info' menu button for Hosted App windows.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, AppInfoOpensPageInfo) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kExampleURL);
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);
  Browser* app_browser = LaunchAppBrowser(app);

  bool dialog_created = false;

  GetPageInfoDialogCreatedCallbackForTesting() = base::BindOnce(
      [](bool* dialog_created) { *dialog_created = true; }, &dialog_created);

  chrome::ExecuteCommand(app_browser, IDC_WEB_APP_MENU_APP_INFO);

  EXPECT_TRUE(dialog_created);

  // The test closure should have run. But clear the global in case it hasn't.
  EXPECT_FALSE(GetPageInfoDialogCreatedCallbackForTesting());
  GetPageInfoDialogCreatedCallbackForTesting().Reset();
}
#endif

// Common app manifest for HostedAppProcessModelTests.
constexpr const char kHostedAppProcessModelManifest[] =
    R"( { "name": "Hosted App Process Model Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["*://app.site.com/frame_tree",  "*://isolated.site.com/"]
            }
          } )";

// This set of tests verifies the hosted app process model behavior in various
// isolation modes.
//
// Relevant frames in the tests:
// - |app| - app.site.com/frame_tree/cross_origin_but_same_site_frames.html
//           Main frame, launch URL of the hosted app (i.e. app.launch.web_url).
// - |same_dir| - app.site.com/frame_tree/simple.htm
//                Another URL, but still covered by hosted app's web extent
//                (i.e. by app.urls).
// - |diff_dir| - app.site.com/save_page/a.htm
//                Same origin as |same_dir| and |app|, but not covered by app's
//                extent.
// - |same_site| - other.site.com/title1.htm
//                 Different origin, but same site as |app|, |same_dir|,
//                 |diff_dir|.
// - |isolated| - isolated.site.com/title1.htm
//                Within app's extent, but belongs to an isolated origin.
//                Some tests also use isolated.site.com/title1.htm (defined by
//                |isolated_url_outside_app_|), which is an isolated origin
//                outside the app's extent.
// - |cross_site| - cross.domain.com/title1.htm
//                  Cross-site from all the other frames.

class HostedAppProcessModelTest : public HostedAppTest {
 public:
  HostedAppProcessModelTest() {}
  ~HostedAppProcessModelTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HostedAppTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    std::string origin1 =
        embedded_test_server()->GetURL("isolated.site.com", "/").spec();
    std::string origin2 =
        embedded_test_server()->GetURL("isolated.foo.com", "/").spec();
    std::string origin_list =
        base::StringPrintf("%s,%s", origin1.c_str(), origin2.c_str());
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    HostedAppTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();

    should_swap_for_cross_site_ = content::AreAllSitesIsolatedForTesting();

    process_map_ = extensions::ProcessMap::Get(browser()->profile());

    same_dir_url_ = embedded_test_server()->GetURL("app.site.com",
                                                   "/frame_tree/simple.htm");
    diff_dir_url_ =
        embedded_test_server()->GetURL("app.site.com", "/save_page/a.htm");
    same_site_url_ =
        embedded_test_server()->GetURL("other.site.com", "/title1.html");
    isolated_url_ =
        embedded_test_server()->GetURL("isolated.site.com", "/title1.html");
    isolated_url_outside_app_ =
        embedded_test_server()->GetURL("isolated.foo.com", "/title1.html");
    cross_site_url_ =
        embedded_test_server()->GetURL("cross.domain.com", "/title1.html");
  }

  // Opens a popup from |rfh| to |url|, verifies whether it should stay in the
  // same process as |rfh| and whether it should be in an app process, and then
  // closes the popup.
  void TestPopupProcess(RenderFrameHost* rfh,
                        const GURL& url,
                        bool expect_same_process,
                        bool expect_app_process) {
    content::WebContentsAddedObserver tab_added_observer;
    ASSERT_TRUE(
        content::ExecuteScript(rfh, "window.open('" + url.spec() + "');"));
    content::WebContents* new_tab = tab_added_observer.GetWebContents();
    ASSERT_TRUE(new_tab);
    EXPECT_TRUE(WaitForLoadStop(new_tab));
    EXPECT_EQ(url, new_tab->GetLastCommittedURL());
    RenderFrameHost* new_rfh = new_tab->GetMainFrame();

    EXPECT_EQ(expect_same_process, rfh->GetProcess() == new_rfh->GetProcess())
        << " for " << url << " from " << rfh->GetLastCommittedURL();

    EXPECT_EQ(expect_app_process,
              process_map_->Contains(new_rfh->GetProcess()->GetID()))
        << " for " << url << " from " << rfh->GetLastCommittedURL();
    EXPECT_EQ(expect_app_process,
              new_rfh->GetSiteInstance()->GetSiteURL().SchemeIs(
                  extensions::kExtensionScheme))
        << " for " << url << " from " << rfh->GetLastCommittedURL();

    content::WebContentsDestroyedWatcher watcher(new_tab);
    ASSERT_TRUE(content::ExecuteScript(new_rfh, "window.close();"));
    watcher.Wait();
  }

  // Creates a subframe underneath |parent_rfh| to |url|, verifies whether it
  // should stay in the same process as |parent_rfh| and whether it should be in
  // an app process, and returns the subframe RFH.
  RenderFrameHost* TestSubframeProcess(RenderFrameHost* parent_rfh,
                                       const GURL& url,
                                       bool expect_same_process,
                                       bool expect_app_process) {
    return TestSubframeProcess(parent_rfh, url, "", expect_same_process,
                               expect_app_process);
  }

  RenderFrameHost* TestSubframeProcess(RenderFrameHost* parent_rfh,
                                       const GURL& url,
                                       const std::string& element_id,
                                       bool expect_same_process,
                                       bool expect_app_process) {
    WebContents* web_contents = WebContents::FromRenderFrameHost(parent_rfh);
    content::TestNavigationObserver nav_observer(web_contents, 1);
    std::string script = "var f = document.createElement('iframe');";
    if (!element_id.empty())
      script += "f.id = '" + element_id + "';";
    script += "f.src = '" + url.spec() + "';";
    script += "document.body.appendChild(f);";
    EXPECT_TRUE(ExecuteScript(parent_rfh, script));
    nav_observer.Wait();

    RenderFrameHost* subframe = content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameHasSourceUrl, url));

    EXPECT_EQ(expect_same_process,
              parent_rfh->GetProcess() == subframe->GetProcess())
        << " for " << url << " from " << parent_rfh->GetLastCommittedURL();

    EXPECT_EQ(expect_app_process,
              process_map_->Contains(subframe->GetProcess()->GetID()))
        << " for " << url << " from " << parent_rfh->GetLastCommittedURL();
    EXPECT_EQ(expect_app_process,
              subframe->GetSiteInstance()->GetSiteURL().SchemeIs(
                  extensions::kExtensionScheme))
        << " for " << url << " from " << parent_rfh->GetLastCommittedURL();

    return subframe;
  }

 protected:
  bool should_swap_for_cross_site_;

  extensions::ProcessMap* process_map_;

  GURL same_dir_url_;
  GURL diff_dir_url_;
  GURL same_site_url_;
  GURL isolated_url_;
  GURL isolated_url_outside_app_;
  GURL cross_site_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedAppProcessModelTest);
};

// Tests that same-site iframes stay inside the hosted app process, even when
// they are not within the hosted app's extent.  This allows same-site scripting
// to work and avoids unnecessary OOPIFs.  Also tests that isolated origins in
// iframes do not stay in the app's process, nor do cross-site iframes in modes
// that require them to swap.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest, IframesInsideHostedApp) {
  // Set up and launch the hosted app.
  GURL url = embedded_test_server()->GetURL(
      "app.site.com", "/frame_tree/cross_origin_but_same_site_frames.html");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kHostedAppProcessModelManifest, url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  auto find_frame = [web_contents](const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameMatchesName, name));
  };
  RenderFrameHost* app = web_contents->GetMainFrame();
  RenderFrameHost* same_dir = find_frame("SameOrigin-SamePath");
  RenderFrameHost* diff_dir = find_frame("SameOrigin-DifferentPath");
  RenderFrameHost* same_site = find_frame("OtherSubdomain-SameSite");
  RenderFrameHost* isolated = find_frame("Isolated-SameSite");
  RenderFrameHost* cross_site = find_frame("CrossSite");

  // Sanity-check sites of all relevant frames to verify test setup.
  GURL app_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), app->GetLastCommittedURL());
  EXPECT_EQ(extensions::kExtensionScheme, app_site.scheme());

  GURL same_dir_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), same_dir->GetLastCommittedURL());
  EXPECT_EQ(extensions::kExtensionScheme, same_dir_site.scheme());
  EXPECT_EQ(same_dir_site, app_site);

  GURL diff_dir_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), diff_dir->GetLastCommittedURL());
  EXPECT_NE(extensions::kExtensionScheme, diff_dir_site.scheme());
  EXPECT_NE(diff_dir_site, app_site);

  GURL same_site_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), same_site->GetLastCommittedURL());
  EXPECT_NE(extensions::kExtensionScheme, same_site_site.scheme());
  EXPECT_NE(same_site_site, app_site);
  EXPECT_EQ(same_site_site, diff_dir_site);

  // The isolated.site.com iframe is covered by the hosted app's extent, so it
  // uses a chrome-extension site URL, but the site URL should be different
  // from the main app's site URL, as this iframe is expected to go into a
  // separate app process, because isolated.site.com matches an isolated
  // origin.
  GURL isolated_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), isolated->GetLastCommittedURL());
  EXPECT_EQ(extensions::kExtensionScheme, isolated_site.scheme());
  EXPECT_NE(isolated_site, app_site);
  EXPECT_NE(isolated_site, diff_dir_site);

  GURL cross_site_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), cross_site->GetLastCommittedURL());
  EXPECT_NE(cross_site_site, app_site);
  EXPECT_NE(cross_site_site, same_site_site);

  // Verify that |same_dir| and |diff_dir| have the same origin according to
  // |window.origin| (even though they have different |same_dir_site| and
  // |diff_dir_site|).
  std::string same_dir_origin;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      same_dir, "domAutomationController.send(window.origin)",
      &same_dir_origin));
  std::string diff_dir_origin;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      diff_dir, "domAutomationController.send(window.origin)",
      &diff_dir_origin));
  EXPECT_EQ(diff_dir_origin, same_dir_origin);

  // Verify that (1) all same-site iframes stay in the process, (2) isolated
  // origin iframe does not, and (3) cross-site iframe leaves if the process
  // model calls for it.
  EXPECT_EQ(same_dir->GetProcess(), app->GetProcess());
  EXPECT_EQ(diff_dir->GetProcess(), app->GetProcess());
  EXPECT_EQ(same_site->GetProcess(), app->GetProcess());
  EXPECT_NE(isolated->GetProcess(), app->GetProcess());
  if (should_swap_for_cross_site_)
    EXPECT_NE(cross_site->GetProcess(), app->GetProcess());
  else
    EXPECT_EQ(cross_site->GetProcess(), app->GetProcess());

  // The isolated origin iframe's process should be in the ProcessMap, since
  // the isolated origin is covered by the app's extent.
  EXPECT_TRUE(process_map_->Contains(isolated->GetProcess()->GetID()));

  // If we swapped processes for the |cross_site| iframe, its process should
  // not be on the ProcessMap.
  if (should_swap_for_cross_site_)
    EXPECT_FALSE(process_map_->Contains(cross_site->GetProcess()->GetID()));

  // Verify that |same_dir| and |diff_dir| can script each other.
  // (they should - they have the same origin).
  std::string inner_text_from_other_frame;
  const std::string r_script =
      R"( var w = window.open('', 'SameOrigin-SamePath');
          domAutomationController.send(w.document.body.innerText); )";
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      diff_dir, r_script, &inner_text_from_other_frame));
  EXPECT_EQ("Simple test page.", inner_text_from_other_frame);
}

// Check that if a hosted app has an iframe, and that iframe navigates to URLs
// that are same-site with the app, these navigations ends up in the app
// process.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest,
                       IframeNavigationsInsideHostedApp) {
  // Set up and launch the hosted app.
  GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/frame_tree/simple.htm");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(base::StringPrintf(kHostedAppProcessModelManifest,
                                                app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  RenderFrameHost* app = web_contents->GetMainFrame();

  // Add a data: URL subframe.  This should stay in the app process.
  TestSubframeProcess(app, GURL("data:text/html,foo"), "test_iframe",
                      true /* expect_same_process */,
                      true /* expect_app_process */);

  // Navigate iframe to a non-app-but-same-site-with-app URL and check that it
  // stays in the parent process.
  {
    SCOPED_TRACE("... for data: -> diff_dir");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", diff_dir_url_));
    EXPECT_EQ(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate the iframe to an isolated origin to force an OOPIF.
  {
    SCOPED_TRACE("... for diff_dir -> isolated");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", isolated_url_));
    EXPECT_NE(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate the iframe to an app URL. This should go back to the app process.
  {
    SCOPED_TRACE("... for isolated -> same_dir");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", same_dir_url_));
    EXPECT_EQ(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate the iframe back to the OOPIF again.
  {
    SCOPED_TRACE("... for same_dir -> isolated");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", isolated_url_));
    EXPECT_NE(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate iframe to a non-app-but-same-site-with-app URL and check that it
  // also goes back to the parent process.
  {
    SCOPED_TRACE("... for isolated -> diff_dir");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", diff_dir_url_));
    EXPECT_EQ(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }
}

// Tests that popups opened within a hosted app behave as expected.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest, PopupsInsideHostedApp) {
  // Set up and launch the hosted app.
  GURL url = embedded_test_server()->GetURL(
      "app.site.com", "/frame_tree/cross_origin_but_same_site_frames.html");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kHostedAppProcessModelManifest, url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  auto find_frame = [web_contents](const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameMatchesName, name));
  };
  RenderFrameHost* app = web_contents->GetMainFrame();
  RenderFrameHost* same_dir = find_frame("SameOrigin-SamePath");
  RenderFrameHost* diff_dir = find_frame("SameOrigin-DifferentPath");
  RenderFrameHost* same_site = find_frame("OtherSubdomain-SameSite");
  RenderFrameHost* isolated = find_frame("Isolated-SameSite");
  RenderFrameHost* cross_site = find_frame("CrossSite");

  {
    SCOPED_TRACE("... for same_dir popup");
    TestPopupProcess(app, same_dir_url_, true, true);
  }
  {
    SCOPED_TRACE("... for diff_dir popup");
    TestPopupProcess(app, diff_dir_url_, true, true);
  }
  {
    SCOPED_TRACE("... for same_site popup");
    TestPopupProcess(app, same_site_url_, true, true);
  }
  // The isolated origin URL for isolated.site.com should swap processes, but
  // since it's covered by the app's extent, it should still be in a
  // (different) app process.
  {
    SCOPED_TRACE("... for isolated_url popup");
    TestPopupProcess(app, isolated_url_, false, true);
  }
  // The isolated origin URL for isolated.foo.com should swap processes, and
  // since it's not covered by the app's extent, it should not be in an app
  // process.
  {
    SCOPED_TRACE("... for isolated_url_outside_app popup");
    TestPopupProcess(app, isolated_url_outside_app_, false, false);
  }
  // For cross-site, the resulting popup should swap processes and not be in
  // the app process.
  {
    SCOPED_TRACE("... for cross_site popup");
    TestPopupProcess(app, cross_site_url_, false, false);
  }

  // If the iframes open popups that are same-origin with themselves, the popups
  // should be in the same process as the respective iframes.
  {
    SCOPED_TRACE("... for same_dir iframe popup");
    TestPopupProcess(same_dir, same_dir_url_, true, true);
  }
  {
    SCOPED_TRACE("... for diff_dir iframe popup");
    TestPopupProcess(diff_dir, diff_dir_url_, true, true);
  }
  {
    SCOPED_TRACE("... for same_site iframe popup");
    TestPopupProcess(same_site, same_site_url_, true, true);
  }
  {
    SCOPED_TRACE("... for isolated_url iframe popup");
    TestPopupProcess(isolated, isolated_url_, true, true);
  }
  {
    SCOPED_TRACE("... for cross_site iframe popup");
    TestPopupProcess(cross_site, cross_site_url_, true,
                     !should_swap_for_cross_site_);
  }
}

// Tests that hosted app URLs loaded in iframes of non-app pages won't cause an
// OOPIF unless there is another reason to create it, but popups from outside
// the app will swap into the app.
// TODO(crbug.com/807471): Flaky on Windows 7.
#if defined(OS_WIN)
#define MAYBE_FromOutsideHostedApp DISABLED_FromOutsideHostedApp
#else
#define MAYBE_FromOutsideHostedApp FromOutsideHostedApp
#endif
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest, MAYBE_FromOutsideHostedApp) {
  // Set up and launch the hosted app.
  GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/frame_tree/simple.htm");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(base::StringPrintf(kHostedAppProcessModelManifest,
                                                app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Starting same-origin but outside the app, popups should swap to the app.
  {
    SCOPED_TRACE("... from diff_dir");
    ui_test_utils::NavigateToURL(app_browser_, diff_dir_url_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should not swap.
    RenderFrameHost* diff_dir_rfh =
        TestSubframeProcess(main_frame, app_url, true, false);
    // Popups from the subframe, though same-origin, should swap to the app.
    // See https://crbug.com/89272.
    TestPopupProcess(diff_dir_rfh, app_url, false, true);
  }

  // Starting same-site but outside the app, popups should swap to the app.
  {
    SCOPED_TRACE("... from same_site");
    ui_test_utils::NavigateToURL(app_browser_, same_site_url_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should not swap.
    RenderFrameHost* same_site_rfh =
        TestSubframeProcess(main_frame, app_url, true, false);
    // Popups from the subframe should swap to the app, as above.
    TestPopupProcess(same_site_rfh, app_url, false, true);
  }

  // Starting on an isolated origin outside the app's extent, popups should
  // swap to the app.
  {
    SCOPED_TRACE("... from isolated_url");
    ui_test_utils::NavigateToURL(app_browser_, isolated_url_outside_app_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should swap process.
    // TODO(creis): Perhaps this OOPIF should not be an app process?
    RenderFrameHost* isolated_rfh =
        TestSubframeProcess(main_frame, app_url, false, true);
    // Popups from the subframe into the app should be in the app process.
    TestPopupProcess(isolated_rfh, app_url, true, true);
  }

  // Starting cross-site, popups should swap to the app.
  {
    SCOPED_TRACE("... from cross_site");
    ui_test_utils::NavigateToURL(app_browser_, cross_site_url_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should swap if the process model needs it.
    // TODO(creis): Perhaps this OOPIF should not be an app process?
    RenderFrameHost* cross_site_rfh =
        TestSubframeProcess(main_frame, app_url, !should_swap_for_cross_site_,
                            should_swap_for_cross_site_);
    // Popups from the subframe into the app should be in the app process.
    TestPopupProcess(cross_site_rfh, app_url, should_swap_for_cross_site_,
                     true);
  }
}

// Helper class that sets up two isolated origins, where one is a subdomain of
// the other: https://isolated.com and https://very.isolated.com.
class HostedAppIsolatedOriginTest : public HostedAppProcessModelTest {
 public:
  HostedAppIsolatedOriginTest() {}
  ~HostedAppIsolatedOriginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HostedAppTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    GURL isolated_url = embedded_test_server()->GetURL("isolated.com", "/");
    GURL very_isolated_url =
        embedded_test_server()->GetURL("very.isolated.com", "/");
    std::string origin_list = base::StringPrintf(
        "%s,%s", isolated_url.spec().c_str(), very_isolated_url.spec().c_str());
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }
};

// Check that a hosted app that is contained within an isolated.com isolated
// origin is allowed to load in a privileged app process. Also check that a
// very.isolated.com URL, which corresponds to very.isolated.com isolated
// origin but is outside the hosted app's extent, ends up in its own non-app
// process. See https://crbug.com/799638.
IN_PROC_BROWSER_TEST_P(HostedAppIsolatedOriginTest,
                       NestedIsolatedOriginStaysOutsideApp) {
  // Set up and launch the hosted app.
  GURL app_url =
      embedded_test_server()->GetURL("isolated.com", "/frame_tree/simple.htm");

  constexpr const char kHostedAppWithinIsolatedOriginManifest[] =
      R"( { "name": "Hosted App Within Isolated Origin Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["http://*.isolated.com/frame_tree"]
            }
          } )";
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(base::StringPrintf(
      kHostedAppWithinIsolatedOriginManifest, app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Check that the app loaded properly. Even though its URL is from an
  // isolated origin (isolated.com), it should go into an app process.
  RenderFrameHost* app = web_contents->GetMainFrame();
  EXPECT_EQ(extensions::kExtensionScheme,
            app->GetSiteInstance()->GetSiteURL().scheme());
  GURL app_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), app->GetLastCommittedURL());
  EXPECT_EQ(extensions::kExtensionScheme, app_site.scheme());
  EXPECT_TRUE(process_map_->Contains(app->GetProcess()->GetID()));

  // Add a same-site subframe on isolated.com outside the app's extent.  This
  // should stay in app process.
  GURL foo_isolated_url =
      embedded_test_server()->GetURL("foo.isolated.com", "/title1.html");
  TestSubframeProcess(app, foo_isolated_url, true /* expect_same_process */,
                      true /* expect_app_process */);

  // Add a subframe on very.isolated.com outside the app's extent.  Despite
  // being same-site, this matches a different, more specific isolated origin
  // and should go into a separate, non-app process.
  GURL very_isolated_url =
      embedded_test_server()->GetURL("very.isolated.com", "/title2.html");
  TestSubframeProcess(app, very_isolated_url, false /* expect_same_process */,
                      false /* expect_app_process */);

  // Add a subframe on very.isolated.com inside the app's extent.  Despite
  // being same-site, this matches a different, more specific isolated origin
  // and should go into a separate app process.
  GURL very_isolated_app_url = embedded_test_server()->GetURL(
      "very.isolated.com", "/frame_tree/simple.htm");
  TestSubframeProcess(app, very_isolated_app_url,
                      false /* expect_same_process */,
                      true /* expect_app_process */);

  // Similarly, a popup for very.isolated.com should go into a separate,
  // non-app process.
  TestPopupProcess(app, very_isolated_url, false /* expect_same_process */,
                   false /* expect_app_process */);

  // Navigating main frame from the app to very.isolated.com should also swap
  // processes to a non-app process.
  ui_test_utils::NavigateToURL(app_browser_, very_isolated_url);
  EXPECT_FALSE(process_map_->Contains(
      web_contents->GetMainFrame()->GetProcess()->GetID()));

  // Navigating main frame back to the app URL should go into an app process.
  ui_test_utils::NavigateToURL(app_browser_, app_url);
  EXPECT_TRUE(process_map_->Contains(
      web_contents->GetMainFrame()->GetProcess()->GetID()));
}

// Check that when a hosted app's extent contains multiple origins, one of
// which is an isolated origin, loading an app URL in that isolated origin
// won't later allow another origin in the app's extent to share the same app
// process.
IN_PROC_BROWSER_TEST_P(HostedAppIsolatedOriginTest,
                       AppBroaderThanIsolatedOrigin) {
  // Set up and launch the hosted app, with the launch URL being in an isolated
  // origin.
  GURL app_url =
      embedded_test_server()->GetURL("isolated.com", "/frame_tree/simple.htm");

  constexpr const char kHostedAppBroaderThanIsolatedOriginManifest[] =
      R"( { "name": "Hosted App Within Isolated Origin Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["http://*.isolated.com/frame_tree", "*://unisolated.com/"]
            }
          } )";
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(base::StringPrintf(
      kHostedAppBroaderThanIsolatedOriginManifest, app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath());

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // The app URL should have loaded in an app process.
  RenderFrameHost* app = web_contents->GetMainFrame();
  EXPECT_TRUE(process_map_->Contains(app->GetProcess()->GetID()));
  EXPECT_EQ(extensions::kExtensionScheme,
            app->GetSiteInstance()->GetSiteURL().scheme());
  int first_app_process_id = app->GetProcess()->GetID();

  // Creating a subframe on unisolated.com should not be allowed to share the
  // main frame's app process, since we don't want the isolated.com isolated
  // origin to share a process with another origin.
  GURL unisolated_app_url =
      embedded_test_server()->GetURL("unisolated.com", "/title1.html");
  TestSubframeProcess(app, unisolated_app_url, false /* expect_same_process */,
                      true /* expect_app_process */);

  // Opening a popup or navigating to an app URL on unisolated.com should go
  // into a separate app process, different from the one that was used for
  // isolated.com.
  TestPopupProcess(app, unisolated_app_url, false /* expect_same_process */,
                   true /* expect_app_process */);

  ui_test_utils::NavigateToURL(app_browser_, unisolated_app_url);
  EXPECT_TRUE(process_map_->Contains(
      web_contents->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_NE(first_app_process_id,
            web_contents->GetMainFrame()->GetProcess()->GetID());
}

class HostedAppSitePerProcessTest : public HostedAppProcessModelTest {
 public:
  HostedAppSitePerProcessTest() {}
  ~HostedAppSitePerProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HostedAppTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::IsolateAllSitesForTesting(command_line);
  }
};

// Check that two different cross-site hosted apps won't share a process even
// when over process limit, when in --site-per-process mode.  See
// https://crbug.com/811939.
IN_PROC_BROWSER_TEST_P(HostedAppSitePerProcessTest,
                       DoNotShareProcessWhenOverProcessLimit) {
  // Set the process limit to 1.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  // Set up and launch a hosted app covering foo.com.
  GURL foo_app_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  constexpr const char kHostedAppManifest[] =
      R"( { "name": "Hosted App With SitePerProcess Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["http://%s/"]
            }
          } )";
  {
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(base::StringPrintf(
        kHostedAppManifest, foo_app_url.spec().c_str(), "foo.com"));
    SetupApp(test_app_dir.UnpackedPath());
  }
  content::WebContents* foo_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(foo_contents));

  // Set up and launch a hosted app covering bar.com.
  GURL bar_app_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  {
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(base::StringPrintf(
        kHostedAppManifest, bar_app_url.spec().c_str(), "bar.com"));
    SetupApp(test_app_dir.UnpackedPath());
  }
  content::WebContents* bar_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(bar_contents));

  EXPECT_NE(foo_contents, bar_contents);
  EXPECT_NE(foo_contents->GetMainFrame()->GetSiteInstance(),
            bar_contents->GetMainFrame()->GetSiteInstance());
  EXPECT_EQ(foo_app_url, foo_contents->GetLastCommittedURL());
  EXPECT_EQ(bar_app_url, bar_contents->GetLastCommittedURL());

  // Under --site-per-process the two apps should load in separate processes,
  // even when over process limit.
  EXPECT_NE(foo_contents->GetMainFrame()->GetProcess(),
            bar_contents->GetMainFrame()->GetProcess());
}

// Check that when a hosted app covers multiple sites in its web extent, these
// sites do not share a process in site-per-process mode. See
// https://crbug.com/791796.
IN_PROC_BROWSER_TEST_P(HostedAppSitePerProcessTest,
                       DoNotShareProcessForDifferentSitesCoveredBySameApp) {
  // Set up a hosted app covering http://foo.com and http://bar.com, and launch
  // the app with a foo.com URL in a new window.
  GURL foo_app_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  constexpr const char kHostedAppManifest[] =
      R"( { "name": "Hosted App With SitePerProcess Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["http://%s/", "http://%s/"]
            }
          } )";
  {
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(base::StringPrintf(
        kHostedAppManifest, foo_app_url.spec().c_str(), "foo.com", "bar.com"));
    SetupApp(test_app_dir.UnpackedPath());
  }
  content::WebContents* foo_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(foo_contents));
  EXPECT_EQ(foo_app_url, foo_contents->GetLastCommittedURL());

  // Now navigate original window to a bar.com app URL.
  GURL bar_app_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  ui_test_utils::NavigateToURL(browser(), bar_app_url);
  content::WebContents* bar_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(bar_app_url, bar_contents->GetLastCommittedURL());
  EXPECT_NE(foo_contents, bar_contents);

  // Ensure the two pages don't share a process despite being from the same
  // app, since they are from different sites.
  EXPECT_NE(foo_contents->GetMainFrame()->GetSiteInstance(),
            bar_contents->GetMainFrame()->GetSiteInstance());
  auto* foo_process =
      foo_contents->GetMainFrame()->GetSiteInstance()->GetProcess();
  auto* bar_process =
      bar_contents->GetMainFrame()->GetSiteInstance()->GetProcess();
  EXPECT_NE(foo_process, bar_process);

  // Ensure each process only has access to its site's data.
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(
      policy->CanAccessDataForOrigin(foo_process->GetID(), foo_app_url));
  EXPECT_FALSE(
      policy->CanAccessDataForOrigin(foo_process->GetID(), bar_app_url));
  EXPECT_FALSE(
      policy->CanAccessDataForOrigin(bar_process->GetID(), foo_app_url));
  EXPECT_TRUE(
      policy->CanAccessDataForOrigin(bar_process->GetID(), bar_app_url));

  // Both processes should still be app processes.
  auto* process_map = extensions::ProcessMap::Get(browser()->profile());
  EXPECT_TRUE(process_map->Contains(foo_process->GetID()));
  EXPECT_TRUE(process_map->Contains(bar_process->GetID()));
}

// Check that when a hosted app covers multiple sites in its web extent,
// navigating from one of these sites to another swaps processes.
IN_PROC_BROWSER_TEST_P(HostedAppSitePerProcessTest,
                       CrossSiteNavigationsWithinApp) {
  // Set up a hosted app covering http://foo.com/frame_tree and http://bar.com.
  GURL foo_app_url(
      embedded_test_server()->GetURL("foo.com", "/frame_tree/simple.htm"));
  GURL bar_app_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  constexpr const char kHostedAppManifest[] =
      R"( { "name": "Hosted App With SitePerProcess Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["http://foo.com/frame_tree", "http://bar.com/"]
            }
          } )";
  {
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(
        base::StringPrintf(kHostedAppManifest, foo_app_url.spec().c_str()));
    SetupApp(test_app_dir.UnpackedPath());
  }

  // Navigate main window to a foo.com app URL.
  ui_test_utils::NavigateToURL(browser(), foo_app_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(foo_app_url, web_contents->GetLastCommittedURL());
  scoped_refptr<content::SiteInstance> foo_site_instance =
      web_contents->GetMainFrame()->GetSiteInstance();
  auto* foo_process = foo_site_instance->GetProcess();
  auto* process_map = extensions::ProcessMap::Get(browser()->profile());
  EXPECT_TRUE(process_map->Contains(foo_process->GetID()));

  // At this point the main frame process should have access to foo.com data
  // but not bar.com data.
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(
      policy->CanAccessDataForOrigin(foo_process->GetID(), foo_app_url));
  EXPECT_FALSE(
      policy->CanAccessDataForOrigin(foo_process->GetID(), bar_app_url));

  // Ensure the current process is allowed to access cookies.
  EXPECT_TRUE(ExecuteScript(web_contents, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(web_contents, "document.cookie"));

  // Now navigate to a bar.com app URL in the same BrowsingInstance.  Ensure
  // that this uses a new SiteInstance and process.
  {
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(
        ExecuteScript(web_contents, "location = '" + bar_app_url.spec() + "'"));
    observer.Wait();
  }
  EXPECT_EQ(bar_app_url, web_contents->GetLastCommittedURL());
  scoped_refptr<content::SiteInstance> bar_site_instance =
      web_contents->GetMainFrame()->GetSiteInstance();
  EXPECT_NE(foo_site_instance, bar_site_instance);
  auto* bar_process = bar_site_instance->GetProcess();
  EXPECT_TRUE(process_map->Contains(bar_process->GetID()));
  EXPECT_NE(foo_process, bar_process);

  // At this point the main frame process should have access to bar.com data.
  EXPECT_TRUE(
      policy->CanAccessDataForOrigin(bar_process->GetID(), bar_app_url));
  EXPECT_FALSE(
      policy->CanAccessDataForOrigin(bar_process->GetID(), foo_app_url));

  // Ensure the current process is allowed to access cookies.
  EXPECT_TRUE(ExecuteScript(web_contents, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(web_contents, "document.cookie"));

  // Now navigate from a foo.com app URL to a foo.com non-app URL.  Ensure that
  // there's a process swap from an app to a non-app process.
  ui_test_utils::NavigateToURL(browser(), foo_app_url);
  EXPECT_EQ(foo_app_url, web_contents->GetLastCommittedURL());
  foo_site_instance = web_contents->GetMainFrame()->GetSiteInstance();
  foo_process = foo_site_instance->GetProcess();
  EXPECT_TRUE(process_map->Contains(foo_process->GetID()));

  GURL foo_nonapp_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  {
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(ExecuteScript(web_contents,
                              "location = '" + foo_nonapp_url.spec() + "'"));
    observer.Wait();
  }
  EXPECT_EQ(foo_nonapp_url, web_contents->GetLastCommittedURL());
  EXPECT_NE(foo_site_instance, web_contents->GetMainFrame()->GetSiteInstance());
  auto* foo_nonapp_process = web_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(foo_process, foo_nonapp_process);
  EXPECT_FALSE(process_map->Contains(foo_nonapp_process->GetID()));

  // Ensure the current non-app foo.com process is allowed to access foo.com
  // data.
  EXPECT_TRUE(policy->CanAccessDataForOrigin(foo_nonapp_process->GetID(),
                                             foo_nonapp_url));
  EXPECT_TRUE(ExecuteScript(web_contents, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(web_contents, "document.cookie"));
}

// Check background page scriptability for a hosted app that covers multiple
// sites in its web extent.  When a hosted app page opens a background page,
// only same-site parts of the app should be able to script that background
// page. This behavior should be the same with and without --site-per-process.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest,
                       BackgroundPageWithAppCoveringDifferentSites) {
  // Set up a hosted app covering http://foo.com and http://bar.com.
  GURL foo_app_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  constexpr const char kHostedAppManifest[] =
      R"( { "name": "Hosted App With SitePerProcess Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["http://foo.com/", "http://bar.com/"]
            },
            "permissions": ["background"]
          } )";
  {
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(
        base::StringPrintf(kHostedAppManifest, foo_app_url.spec().c_str()));
    SetupApp(test_app_dir.UnpackedPath());
  }

  // Set up three unrelated hosted app tabs in the main browser window:
  // foo.com, bar.com, and another one at foo.com.
  ui_test_utils::NavigateToURL(browser(), foo_app_url);
  content::WebContents* foo_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(foo_app_url, foo_contents->GetLastCommittedURL());

  GURL bar_app_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), bar_app_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* bar_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(bar_app_url, bar_contents->GetLastCommittedURL());
  EXPECT_NE(foo_contents, bar_contents);

  GURL foo_app_url2(embedded_test_server()->GetURL("foo.com", "/title3.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), foo_app_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* foo_contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(foo_app_url2, foo_contents2->GetLastCommittedURL());
  EXPECT_NE(foo_contents, foo_contents2);
  EXPECT_NE(bar_contents, foo_contents2);
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // The two foo.com tabs should be in the same process even though they are
  // unrelated, since hosted apps use the process-per-site process model.
  auto* foo_process = foo_contents->GetMainFrame()->GetProcess();
  EXPECT_EQ(foo_process, foo_contents2->GetMainFrame()->GetProcess());
  EXPECT_FALSE(
      foo_contents->GetMainFrame()->GetSiteInstance()->IsRelatedSiteInstance(
          foo_contents2->GetMainFrame()->GetSiteInstance()));

  // The bar.com tab should be in a different process from the foo.com tabs.
  auto* bar_process = bar_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(foo_process, bar_process);

  // Ensure all tabs are in app processes.
  auto* process_map = extensions::ProcessMap::Get(browser()->profile());
  EXPECT_TRUE(process_map->Contains(foo_process->GetID()));
  EXPECT_TRUE(process_map->Contains(bar_process->GetID()));

  // Open a background page from the first foo.com window.
  {
    content::TestNavigationObserver background_page_observer(nullptr);
    background_page_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(content::ExecuteScript(
        foo_contents,
        "window.bg = window.open('/empty.html', 'bg', 'background');"));
    background_page_observer.Wait();
    EXPECT_EQ(embedded_test_server()->GetURL("foo.com", "/empty.html"),
              background_page_observer.last_navigation_url());

    // The background page shouldn't show up in the tab strip.
    ASSERT_EQ(3, browser()->tab_strip_model()->count());
  }

  // Script the background page from the first foo.com window and set a dummy
  // value.
  EXPECT_TRUE(content::ExecuteScript(foo_contents,
                                     "bg.document.body.innerText = 'foo'"));

  // Ensure that the second foo.com page can script the same background page
  // and retrieve the value.
  EXPECT_EQ("foo",
            content::EvalJs(foo_contents2,
                            "window.open('', 'bg').document.body.innerText"));

  // Ensure that the bar.com page cannot script this background page, since it
  // is cross-origin from it. The window lookup via window.open('', bg') should
  // be disallowed, resulting in a new popup instead, and the innerText value
  // from that should be empty.
  EXPECT_EQ("",
            content::EvalJs(bar_contents,
                            "window.open('', 'bg').document.body.innerText"));

  // Open another bar.com app URL in an unrelated tab.  This should share a
  // process with the first bar.com tab, due to hosted apps using
  // process-per-site.
  GURL bar_app_url2(embedded_test_server()->GetURL("bar.com", "/title3.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), bar_app_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* bar_contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(bar_app_url2, bar_contents2->GetLastCommittedURL());
  EXPECT_EQ(bar_process, bar_contents2->GetMainFrame()->GetProcess());
  EXPECT_FALSE(
      bar_contents->GetMainFrame()->GetSiteInstance()->IsRelatedSiteInstance(
          bar_contents2->GetMainFrame()->GetSiteInstance()));

  // Ensure bar.com tabs can open and script their open background page.
  {
    content::TestNavigationObserver background_page_observer(nullptr);
    background_page_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(content::ExecuteScript(
        bar_contents,
        "window.bg = window.open('/empty.html', 'bg2', 'background');"));
    background_page_observer.Wait();
    EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/empty.html"),
              background_page_observer.last_navigation_url());
  }
  EXPECT_TRUE(content::ExecuteScript(bar_contents,
                                     "bg.document.body.innerText = 'bar'"));
  EXPECT_EQ("bar",
            content::EvalJs(bar_contents2,
                            "window.open('', 'bg2').document.body.innerText"));
}

// Check that toolbar is not shown for apps hosted within extensions pages.
// This simulates a case where the user has manually navigated to a page hosted
// within an extension, then added it as a bookmark app.
// Regression test for https://crbug.com/828233.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest,
                       ShouldShowCustomTabBarForExtensionPage) {
  // Note: This involves the creation of *two* extensions: The first is a
  // regular (non-app) extension with a popup page. The second is a bookmark app
  // created from the popup page URL (allowing the extension's popup page to be
  // loaded in a window).

  // Install the extension that has the popup page.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Install the bookmark app that links to the extension's popup page.
  GURL popup_url("chrome-extension://" + last_loaded_extension_id() +
                 "/popup.html");
  // TODO(mgiuca): Abstract this logic to share code with InstallPWA (which does
  // almost the same thing, but also sets a scope).
  WebApplicationInfo web_app_info;
  web_app_info.app_url = popup_url;
  app_ = InstallBookmarkApp(web_app_info);

  ui_test_utils::UrlLoadObserver url_observer(
      popup_url, content::NotificationService::AllSources());
  app_browser_ = LaunchAppBrowser(app_);
  url_observer.Wait();

  CHECK(app_browser_);
  CHECK(app_browser_ != browser());

  // Navigate to the app's launch page; the toolbar should not be visible,
  // because extensions pages are secure.
  NavigateAndCheckForToolbar(app_browser_, popup_url, false);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HostedAppTest,
                         ::testing::Values(AppType::HOSTED_APP,
                                           AppType::BOOKMARK_APP));

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SharedAppTest,
                         ::testing::Values(AppType::HOSTED_APP,
                                           AppType::BOOKMARK_APP,
                                           AppType::WEB_APP));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HostedAppPWAOnlyTest,
    ::testing::Values(AppType::BOOKMARK_APP));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SharedPWATest,
    ::testing::Values(AppType::BOOKMARK_APP, AppType::WEB_APP));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HostedAppProcessModelTest,
    ::testing::Values(AppType::HOSTED_APP));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HostedAppIsolatedOriginTest,
    ::testing::Values(AppType::HOSTED_APP));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HostedAppSitePerProcessTest,
    ::testing::Values(AppType::HOSTED_APP));
