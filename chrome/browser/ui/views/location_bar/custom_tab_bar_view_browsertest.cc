// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/views/controls/button/image_button.h"

namespace {

// Waits until the title of any tab in the browser for |contents| has the title
// |target_title|.
class TestTitleObserver : public TabStripModelObserver {
 public:
  // Create a new TitleObserver for the browser of |contents|, waiting for
  // |target_title|.
  TestTitleObserver(content::WebContents* contents, std::u16string target_title)
      : contents_(contents), target_title_(target_title) {
    browser_ = chrome::FindBrowserWithTab(contents_);
    browser_->tab_strip_model()->AddObserver(this);
  }

  // Run a loop, blocking until a tab has the title |target_title|.
  void Wait() {
    if (seen_target_title_)
      return;

    awaiter_.Run();
  }

  // TabstripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    std::u16string title = entry ? entry->GetTitle() : std::u16string();

    if (title != target_title_)
      return;

    seen_target_title_ = true;
    awaiter_.Quit();
  }

 private:
  bool seen_target_title_ = false;

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> contents_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_;
  std::u16string target_title_;
  base::RunLoop awaiter_;
};

// Opens a new popup window from |web_contents| on |target_url| and returns
// the Browser it opened in.
Browser* OpenPopup(content::WebContents* web_contents, const GURL& target_url) {
  ui_test_utils::BrowserChangeObserver browser_change_observer(
      /*browser=*/nullptr,
      ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  content::TestNavigationObserver nav_observer(target_url);
  nav_observer.StartWatchingNewWebContents();

  std::string script = "window.open('" + target_url.spec() +
                       "', 'popup', 'width=400 height=400');";
  EXPECT_TRUE(content::ExecJs(web_contents, script));
  nav_observer.Wait();

  return browser_change_observer.Wait();
}

// Navigates to |target_url| and waits for navigation to complete.
void NavigateAndWait(content::WebContents* web_contents,
                     const GURL& target_url) {
  content::TestNavigationObserver nav_observer(web_contents);

  std::string script = "window.location = '" + target_url.spec() + "';";
  EXPECT_TRUE(content::ExecJs(web_contents, script));
  nav_observer.Wait();
}

// Navigates |web_contents| to |location|, waits for navigation to complete
// and then sets document.title to be |title| and waits for the change
// to propogate.
void SetTitleAndLocation(content::WebContents* web_contents,
                         const std::u16string title,
                         const GURL& location) {
  NavigateAndWait(web_contents, location);

  TestTitleObserver title_observer(web_contents, title);

  std::string script = "document.title = '" + base::UTF16ToASCII(title) + "';";
  EXPECT_TRUE(content::ExecJs(web_contents, script));

  title_observer.Wait();
}

// An interstitial page that requests URL hiding
class UrlHidingInterstitialPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  UrlHidingInterstitialPage(content::WebContents* web_contents,
                            const GURL& request_url)
      : security_interstitials::SecurityInterstitialPage(
            web_contents,
            request_url,
            std::make_unique<
                security_interstitials::SecurityInterstitialControllerClient>(
                web_contents,
                nullptr,
                nullptr,
                base::i18n::GetConfiguredLocale(),
                GURL(),
                nullptr /* settings_page_helper */)) {}
  void OnInterstitialClosing() override {}
  bool ShouldDisplayURL() const override { return false; }

 protected:
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override {
  }
};

// An observer that associates a URL-hiding interstitial when a page loads when
// |install_interstitial| is true.
class UrlHidingWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit UrlHidingWebContentsObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents), install_interstitial_(true) {}

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!install_interstitial_)
      return;

    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle,
                              std::make_unique<UrlHidingInterstitialPage>(
                                  web_contents(), handle->GetURL()));
  }

  void StopBlocking() { install_interstitial_ = false; }

 private:
  bool install_interstitial_;
};

}  // namespace

class CustomTabBarViewBrowserTest : public web_app::WebAppBrowserTestBase {
 public:
  CustomTabBarViewBrowserTest() = default;

  CustomTabBarViewBrowserTest(const CustomTabBarViewBrowserTest&) = delete;
  CustomTabBarViewBrowserTest& operator=(const CustomTabBarViewBrowserTest&) =
      delete;

  ~CustomTabBarViewBrowserTest() override = default;

 protected:

  void SetUpCommandLine(base::CommandLine* command_line) override {
    web_app::WebAppBrowserTestBase::SetUpCommandLine(command_line);
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  }

  void SetUp() override {
    feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
    web_app::WebAppBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    web_app::WebAppBrowserTestBase::SetUpOnMainThread();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());

    location_bar_ = browser_view_->GetLocationBarView();
    custom_tab_bar_ = browser_view_->toolbar()->custom_tab_bar();
  }

  void InstallPWA(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    Install(std::move(web_app_info));
  }

  void InstallBookmark(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.DeprecatedGetOriginAsURL();
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    Install(std::move(web_app_info));
  }

  raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_;
  raw_ptr<LocationBarView, AcrossTasksDanglingUntriaged> location_bar_;
  raw_ptr<CustomTabBarView, AcrossTasksDanglingUntriaged> custom_tab_bar_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> app_browser_ = nullptr;
  raw_ptr<web_app::AppBrowserController, AcrossTasksDanglingUntriaged>
      app_controller_ = nullptr;

 private:
  void Install(std::unique_ptr<web_app::WebAppInstallInfo> web_app_info) {
    const GURL start_url = web_app_info->start_url();
    webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

    ui_test_utils::UrlLoadObserver url_observer(start_url);
    app_browser_ = LaunchWebAppBrowser(app_id);
    url_observer.Wait();

    DCHECK(app_browser_);
    DCHECK(app_browser_ != browser());

    app_controller_ = app_browser_->app_controller();
    DCHECK(app_controller_);
  }

  base::test::ScopedFeatureList feature_list_;
};

// Check the custom tab bar is not instantiated for a tabbed browser window.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       IsNotCreatedInTabbedBrowser) {
  EXPECT_TRUE(browser()->is_type_normal());
  EXPECT_TRUE(browser_view_->GetIsNormalType());
  EXPECT_FALSE(custom_tab_bar_);
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, IsNotCreatedInPopup) {
  Browser* popup = OpenPopup(browser_view_->GetActiveWebContents(),
                             GURL("http://example.com"));
  EXPECT_TRUE(popup);

  BrowserView* popup_view = BrowserView::GetBrowserViewForBrowser(popup);

  // The popup should be in a new window.
  EXPECT_NE(browser_view_, popup_view);

  // Popups are not the normal browser view.
  EXPECT_FALSE(popup_view->GetIsNormalType());
  EXPECT_TRUE(popup->is_type_popup());
  // Popups should not have a custom tab bar view.
  EXPECT_FALSE(popup_view->toolbar()->custom_tab_bar());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       BackToAppButtonIsNotVisibleInOutOfScopePopups) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  const GURL out_of_scope_url = GURL("https://example.com");

  InstallBookmark(app_url);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);

  Browser* popup_browser =
      OpenPopup(app_view->GetActiveWebContents(), out_of_scope_url);
  EXPECT_TRUE(popup_browser->is_type_app_popup());

  // Out of scope, so custom tab bar should be shown.
  EXPECT_TRUE(popup_browser->app_controller()->ShouldShowCustomTabBar());

  // As the popup was opened out of scope the close button should not be shown.
  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(popup_browser)
                   ->toolbar()
                   ->custom_tab_bar()
                   ->close_button_for_testing()
                   ->GetVisible());
}

// Check the custom tab will be used for a Desktop PWA.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, IsUsedForDesktopPWA) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  EXPECT_FALSE(app_view->GetIsNormalType());
  EXPECT_TRUE(app_browser_->is_type_app());

  // Custom tab bar should be created.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar());
}

// Check the CustomTabBarView appears when a PWA window attempts to load
// insecure content.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, ShowsWithMixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(url);

  ASSERT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  CustomTabBarView* bar = BrowserView::GetBrowserViewForBrowser(app_browser_)
                              ->toolbar()
                              ->custom_tab_bar();
  EXPECT_FALSE(bar->GetVisible());
  EXPECT_TRUE(ExecJs(app_browser_->tab_strip_model()->GetActiveWebContents(),
                     R"(
      let img = document.createElement('img');
      img.src = 'http://not-secure.com';
      document.body.appendChild(img);
    )"));
  EXPECT_TRUE(bar->GetVisible());
  EXPECT_EQ(bar->title_for_testing(), u"Google");
  EXPECT_EQ(bar->location_for_testing() + u"/",
            base::ASCIIToUTF16(https_server()
                                   ->GetURL("app.com", "/ssl")
                                   .DeprecatedGetOriginAsURL()
                                   .spec()));
  EXPECT_FALSE(bar->close_button_for_testing()->GetVisible());
}

// The custom tab bar should update with the title and location of the current
// page.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, TitleAndLocationUpdate) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");

  // This url is out of scope, because the CustomTabBar is not updated when it
  // is not shown.
  const GURL navigate_to = https_server()->GetURL("app.com", "/simple.html");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  SetTitleAndLocation(app_view->GetActiveWebContents(), u"FooBar", navigate_to);

  std::string expected_origin = navigate_to.DeprecatedGetOriginAsURL().spec();
  EXPECT_EQ(
      base::ASCIIToUTF16(expected_origin),
      app_view->toolbar()->custom_tab_bar()->location_for_testing() + u"/");
  EXPECT_EQ(u"FooBar",
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

// If the page doesn't specify a title, we should use the origin.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       UsesLocationInsteadOfEmptyTitles) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  // Empty title should use location.
  SetTitleAndLocation(app_view->GetActiveWebContents(), std::u16string(),
                      GURL("http://example.test/"));
  EXPECT_EQ(u"example.test",
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(u"example.test",
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

// Closing the CCT should take you back to the last in scope url.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       OutOfScopeUrlShouldBeClosable) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  auto* web_contents = app_view->GetActiveWebContents();
  EXPECT_NE(app_view, browser_view_);

  // Perform an inscope navigation.
  const GURL other_app_url =
      https_server()->GetURL("app.com", "/ssl/blank_page.html");
  NavigateAndWait(web_contents, other_app_url);
  EXPECT_FALSE(app_controller_->ShouldShowCustomTabBar());

  // Navigate out of scope.
  NavigateAndWait(web_contents, GURL("http://example.test/"));
  EXPECT_TRUE(app_controller_->ShouldShowCustomTabBar());

  // Simulate clicking the close button and wait for navigation to finish.
  content::TestNavigationObserver nav_observer(web_contents);
  app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
  nav_observer.Wait();

  // The app should be on the last in scope url we visited.
  EXPECT_EQ(other_app_url, web_contents->GetLastCommittedURL());
}

// Right-click menu on CustomTabBar should have Copy URL option.
// Disabled on Mac because Mac's native menu is synchronous.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       RightClickMenuShowsCopyUrl) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  auto* web_contents = app_view->GetActiveWebContents();

  // Navigate out of scope.
  NavigateAndWait(web_contents, GURL("http://example.test/"));
  EXPECT_TRUE(app_controller_->ShouldShowCustomTabBar());

  // Show the right-click context menu.
  app_view->toolbar()->custom_tab_bar()->ShowContextMenu(gfx::Point(),
                                                         ui::MENU_SOURCE_MOUSE);

  content::BrowserTestClipboardScope test_clipboard_scope;
  // Activate the first and only context menu item: IDC_COPY_URL.
  app_view->toolbar()
      ->custom_tab_bar()
      ->context_menu_for_testing()
      ->ActivatedAt(0);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string result;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &result);
  EXPECT_EQ(result, u"http://example.test/");
}
#endif  // !BUILDFLAG(IS_MAC)

// Paths above the launch url should be out of scope and should be closable from
// the CustomTabBar.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       ScopeAboveLaunchURLShouldBeOutOfScopeAndClosable) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  auto* web_contents = app_view->GetActiveWebContents();
  EXPECT_NE(app_view, browser_view_);

  // Navigate to a different page in the app scope, so we have something to come
  // back to.
  const GURL other_app_url =
      https_server()->GetURL("app.com", "/ssl/blank_page.html");
  NavigateAndWait(web_contents, other_app_url);
  EXPECT_FALSE(app_controller_->ShouldShowCustomTabBar());

  // Navigate above the scope of the app, on the same origin.
  NavigateAndWait(web_contents, https_server()->GetURL(
                                    "app.com", "/accessibility_fail.html"));
  EXPECT_TRUE(app_controller_->ShouldShowCustomTabBar());

  // Simulate clicking the close button and wait for navigation to finish.
  content::TestNavigationObserver nav_observer(web_contents);
  app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
  nav_observer.Wait();

  // The app should be on the last in scope url we visited.
  EXPECT_EQ(other_app_url, web_contents->GetLastCommittedURL());
}

// When there are no in scope urls to navigate back to, closing the custom tab
// bar should navigate to the app's launch url.
IN_PROC_BROWSER_TEST_F(
    CustomTabBarViewBrowserTest,
    WhenNoHistoryIsInScopeCloseShouldNavigateToAppLaunchURL) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  auto* web_contents = app_view->GetActiveWebContents();
  EXPECT_NE(app_view, browser_view_);

  {
    // Do a state replacing navigation, so we don't have any in scope urls in
    // history.
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(content::ExecJs(
        web_contents, "window.location.replace('http://example.com');"));
    nav_observer.Wait();
    EXPECT_TRUE(app_controller_->ShouldShowCustomTabBar());
  }
  {
    // Simulate clicking the close button and wait for navigation to finish.
    content::TestNavigationObserver nav_observer(web_contents);
    app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
    nav_observer.Wait();
  }
  // The app should be on the last in scope url we visited.
  EXPECT_EQ(app_url, web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       OriginsWithEmojiArePunyCoded) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  const GURL navigate_to = GURL("https://🔒.example/ssl/blank_page.html");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  SetTitleAndLocation(app_view->GetActiveWebContents(), u"FooBar", navigate_to);

  EXPECT_EQ(u"https://xn--lv8h.example",
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(u"FooBar",
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       OriginsWithNonASCIICharactersDisplayNormally) {
  const GURL app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  const GURL navigate_to = GURL("https://ΐ.example/ssl/blank_page.html");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  SetTitleAndLocation(app_view->GetActiveWebContents(), u"FooBar", navigate_to);

  EXPECT_EQ(u"https://ΐ.example",
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(u"FooBar",
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       BackToAppButtonIsNotVisibleInScope) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // We install over http because it's the easiest way to get a custom tab bar
  // in scope. A PWA won't be installed over http in the real world (it'd make a
  // shortcut app instead).
  const GURL app_url =
      embedded_test_server()->GetURL("app.com", "/ssl/google.html");
  const GURL out_of_scope_url = GURL("https://example.com");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);
  content::WebContents* web_contents = app_view->GetActiveWebContents();

  // Insecure site, so should show custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->GetVisible());
  // In scope, so don't show close button.
  EXPECT_FALSE(app_view->toolbar()
                   ->custom_tab_bar()
                   ->close_button_for_testing()
                   ->GetVisible());

  NavigateAndWait(web_contents, out_of_scope_url);

  // Out of scope, show the custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->GetVisible());
  // Out of scope, show the close button.
  EXPECT_TRUE(app_view->toolbar()
                  ->custom_tab_bar()
                  ->close_button_for_testing()
                  ->GetVisible());

  // Simulate clicking the close button and wait for navigation to finish.
  content::TestNavigationObserver nav_observer(web_contents);
  app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
  nav_observer.Wait();

  // Insecure site, show the custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->GetVisible());
  // In scope, hide the close button.
  EXPECT_FALSE(app_view->toolbar()
                   ->custom_tab_bar()
                   ->close_button_for_testing()
                   ->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       BackToAppButtonIsNotVisibleInBookmarkAppOnOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url =
      embedded_test_server()->GetURL("app.com", "/ssl/google.html");
  const GURL out_of_scope_url = GURL("https://example.com");

  InstallBookmark(app_url);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  // Insecure site, so should show custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->GetVisible());
  // On origin, so don't show close button.
  EXPECT_FALSE(app_view->toolbar()
                   ->custom_tab_bar()
                   ->close_button_for_testing()
                   ->GetVisible());

  NavigateAndWait(app_view->GetActiveWebContents(), out_of_scope_url);

  // Off origin, show the custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->GetVisible());
  // Off origin, show the close button.
  EXPECT_TRUE(app_view->toolbar()
                  ->custom_tab_bar()
                  ->close_button_for_testing()
                  ->GetVisible());
}

// Verify that interstitials that hide origin have their preference respected.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, InterstitialCanHideOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallPWA(https_server()->GetURL("app.com", "/ssl/google.html"));
  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  content::WebContents* contents = app_view->GetActiveWebContents();

  // Verify origin is blanked on interstitial.
  UrlHidingWebContentsObserver blocker(contents);
  SetTitleAndLocation(contents, u"FooBar",
                      https_server()->GetURL("/simple.html"));

  EXPECT_EQ(std::u16string(),
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_FALSE(
      app_view->toolbar()->custom_tab_bar()->IsShowingOriginForTesting());

  // Verify origin returns when interstitial is gone.
  blocker.StopBlocking();
  SetTitleAndLocation(contents, u"FooBar2",
                      https_server()->GetURL("/title1.html"));

  EXPECT_NE(std::u16string(),
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_TRUE(
      app_view->toolbar()->custom_tab_bar()->IsShowingOriginForTesting());
}

// Verify that blob URLs are displayed in the location text.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, BlobUrlLocation) {
  InstallPWA(https_server()->GetURL("/simple.html"));
  EXPECT_TRUE(app_browser_);
  EXPECT_TRUE(app_browser_->is_type_app());
  BrowserView* app_browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_browser_view, browser_view_);
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver nav_observer(web_contents,
                                               /*number_of_navigations=*/1);
  std::string script =
      "window.open("
      "    URL.createObjectURL("
      "        new Blob([], {type: 'text/html'})"
      "    ),"
      "    '_self');";
  EXPECT_TRUE(content::ExecJs(web_contents, script));
  nav_observer.Wait();

  EXPECT_EQ(
      app_browser_view->toolbar()->custom_tab_bar()->location_for_testing() +
          u"/",
      base::ASCIIToUTF16(https_server()->GetURL("/").spec()));
}
