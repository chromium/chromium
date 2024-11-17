// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/screen_base.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::WebContents;

namespace {

const char kExpectedTitle[] = "PASSED!";
const char16_t kExpectedTitle16[] = u"PASSED!";
const char kEchoTitleCommand[] = "/echotitle";

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

GURL GetSettingsURL() {
  return GURL(chrome::kChromeUISettingsURL);
}

GURL GetContentSettingsURL() {
  return GetSettingsURL().Resolve(chrome::kContentSettingsSubPage);
}

GURL GetClearBrowsingDataURL() {
  return GetSettingsURL().Resolve(chrome::kClearBrowserDataSubPage);
}

void ShowSettings(Browser* browser) {
  // chrome::ShowSettings just calls ShowSettingsSubPageInTabbedBrowser on
  // non chromeos, but we want to test tab navigation here so call
  // ShowSettingsSubPageInTabbedBrowser directly.
  chrome::ShowSettingsSubPageInTabbedBrowser(browser, std::string());
}

// Intercepts requests and makes them 302 to the 'redirect' query parameter
// if it's set. If the 'popin-policy' query parameter is set, it sets the
// 'Popin-Policy' response header to that value.
std::unique_ptr<net::test_server::HttpResponse> PopinRequestHandler(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content("<html></html");
  net::test_server::RequestQuery query =
      net::test_server::ParseQuery(request.GetURL());
  if (query.find("redirect") != query.end()) {
    response->AddCustomHeader("Location", query["redirect"][0]);
    response->set_code(net::HTTP_FOUND);
  } else {
    response->set_code(net::HTTP_OK);
  }
  if (query.find("popin_policy") != query.end()) {
    std::string proposed_policy = query["popin_policy"][0];
    // We need to fixup the dynamic port so that the policies can match.
    base::ReplaceSubstringsAfterOffset(&proposed_policy, 0, "a.test",
                                       "a.test:" + request.GetURL().port());
    response->AddCustomHeader("Popin-Policy", "partitioned=" + proposed_policy);
  }
  return response;
}

}  // namespace

BrowserNavigatorTest::BrowserNavigatorTest() {
  scoped_feature_list_.InitWithFeatures(
      {
          features::kFileSystemAccessPersistentPermissions,
          blink::features::kPartitionedPopins,
      },
      {});
}

void BrowserNavigatorTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
}

NavigateParams BrowserNavigatorTest::MakeNavigateParams() const {
  return MakeNavigateParams(browser());
}

NavigateParams BrowserNavigatorTest::MakeNavigateParams(
    Browser* browser) const {
  NavigateParams params(browser, GetGoogleURL(), ui::PAGE_TRANSITION_LINK);
  params.window_action = NavigateParams::SHOW_WINDOW;
  return params;
}

bool BrowserNavigatorTest::OpenPOSTURLInNewForegroundTabAndGetTitle(
    const GURL& url,
    const std::string& post_data,
    bool is_browser_initiated,
    std::u16string* title) {
  NavigateParams param(MakeNavigateParams());
  param.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  param.url = url;
  param.initiator_origin = url::Origin();
  param.is_renderer_initiated = !is_browser_initiated;
  param.post_data = network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());

  ui_test_utils::NavigateToURL(&param);
  if (!param.navigated_or_inserted_contents)
    return false;

  // Navigate() should have opened the contents in new foreground tab in the
  // current Browser.
  EXPECT_EQ(browser(), param.browser);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            param.navigated_or_inserted_contents);
  // We should have one window, with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  *title = param.navigated_or_inserted_contents->GetTitle();
  return true;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForType(Browser::Type type,
                                                         Profile* profile) {
  Browser* browser =
      Browser::Create(Browser::CreateParams(type, profile, true));
  chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForApp(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams::CreateForApp(
      "Test", false /* trusted_source */, gfx::Rect(), profile, true));
  chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

std::unique_ptr<WebContents> BrowserNavigatorTest::CreateWebContents(
    bool initialize_renderer) {
  WebContents::CreateParams create_params(browser()->profile());
  create_params.desired_renderer_state =
      initialize_renderer
          ? WebContents::CreateParams::kInitializeAndWarmupRendererProcess
          : WebContents::CreateParams::kOkayToHaveRendererProcess;
  return WebContents::Create(create_params);
}

void BrowserNavigatorTest::RunSuppressTest(WindowOpenDisposition disposition) {
  GURL old_url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  NavigateParams params(MakeNavigateParams());
  params.disposition = disposition;
  Navigate(&params);

  // Nothing should have happened as a result of Navigate();
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(old_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

void BrowserNavigatorTest::RunUseNonIncognitoWindowTest(
    const GURL& url,
    const ui::PageTransition& page_transition) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());

  // Navigate to the page.
  NavigateParams params(MakeNavigateParams(incognito_browser));
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = url;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.transition = page_transition;
  Navigate(&params);

  // This page should be opened in browser() window.
  EXPECT_NE(incognito_browser, params.browser);
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

void BrowserNavigatorTest::RunDoNothingIfIncognitoIsForcedTest(
    const GURL& url) {
  Browser* browser = CreateIncognitoBrowser();

  // Set kIncognitoModeAvailability to FORCED.
  PrefService* prefs1 = browser->profile()->GetPrefs();
  prefs1->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kForced));
  PrefService* prefs2 = browser->profile()->GetOriginalProfile()->GetPrefs();
  prefs2->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kForced));

  // Navigate to the page.
  NavigateParams params(MakeNavigateParams(browser));
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  params.url = url;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);

  // The page should not be opened.
  EXPECT_EQ(browser, params.browser);
  EXPECT_EQ(1, browser->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Subclass of TestNavigationObserver that saves ChromeNavigationUIData.
class TestNavigationUIDataObserver : public content::TestNavigationObserver {
 public:
  // Creates an observer that watches navigations to |target_url| on
  // existing and newly added WebContents.
  explicit TestNavigationUIDataObserver(const GURL& target_url)
      : content::TestNavigationObserver(target_url) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  const ChromeNavigationUIData* last_navigation_ui_data() const {
    return static_cast<ChromeNavigationUIData*>(last_navigation_ui_data_.get());
  }

 private:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_navigation_ui_data_ =
        navigation_handle->GetNavigationUIData()->Clone();
    content::TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }

  std::unique_ptr<content::NavigationUIData> last_navigation_ui_data_;
};

Browser* BrowserNavigatorTest::NavigateHelper(const GURL& url,
                                              Browser* browser,
                                              WindowOpenDisposition disposition,
                                              bool wait_for_navigation,
                                              WebContents* expected_contents) {
  // If this should navigate the current tab, than assume that the WebContents
  // will be the same one.  This is a convenience for the common case.
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    EXPECT_FALSE(expected_contents);
    expected_contents = browser->tab_strip_model()->GetActiveWebContents();
  }
  std::optional<content::CreateAndLoadWebContentsObserver> new_tab_observer;
  std::optional<content::LoadStopObserver> load_stop_observer;
  if (wait_for_navigation) {
    if (expected_contents)
      load_stop_observer.emplace(expected_contents);
    else
      new_tab_observer.emplace();
  }

  NavigateParams params(MakeNavigateParams(browser));
  params.disposition = disposition;
  params.url = url;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);

  if (load_stop_observer)
    load_stop_observer->Wait();
  if (new_tab_observer)
    new_tab_observer->Wait();

  return params.browser;
}

namespace {

// This test verifies that when a navigation occurs within a tab, the tab count
// of the Browser remains the same and the current tab bears the loaded URL.
// Note that network URLs are not actually loaded in tests, so this also tests
// that error pages leave the intended URL in the address bar.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_CurrentTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleURL()));
  EXPECT_EQ(GetGoogleURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  // We should have one window with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

// This test verifies that a singleton tab is refocused if one is already opened
// in another or an existing window, or added if it is not.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SingletonTabExisting) {
  const GURL singleton_url1("http://maps.google.com/");

  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                ui::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url1.
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url1;
  Navigate(&params);

  // The middle tab should now be selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // No tab contents should have been created
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNoneExisting) {
  const GURL singleton_url1("http://maps.google.com/");

  // We should have one browser with 1 tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url1.
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url1;
  Navigate(&params);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
}

// This test verifies that when a navigation results in a foreground tab, the
// tab count of the Browser increases and the selected tab shifts to the new
// foreground tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewForegroundTab) {
  WebContents* old_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  EXPECT_NE(old_contents, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            params.navigated_or_inserted_contents);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This test verifies that when a navigation results in a background tab, the
// tab count of the Browser increases but the selected tab remains the same.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewBackgroundTab) {
  WebContents* old_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);
  WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // The selected tab should have remained unchanged, since the new tab was
  // opened in the background.
  EXPECT_EQ(old_contents, new_contents);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This test verifies that when a navigation requiring a new foreground tab
// occurs in a Browser that cannot host multiple tabs, the new foreground tab
// is created in an existing compatible Browser.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_IncompatibleWindow_Existing) {
  // Open a foreground tab in a window that cannot open popups when there is an
  // existing compatible window somewhere else that they can be opened within.
  Browser* popup =
      CreateEmptyBrowserForType(Browser::TYPE_POPUP, browser()->profile());
  NavigateParams params(MakeNavigateParams(popup));
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, params.browser);

  // Since browser() is an existing compatible tabbed browser, it should have
  // opened the tab there.
  EXPECT_EQ(browser(), params.browser);

  // We should be left with 2 windows, the popup with one tab and the browser()
  // provided by the framework with two.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, popup->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This test verifies that when a navigation requiring a new foreground tab
// occurs in a Browser that cannot host multiple tabs and no compatible Browser
// that can is open, a compatible Browser is created.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_IncompatibleWindow_NoExisting) {
  // We want to simulate not being able to find an existing window compatible
  // with our non-tabbed browser window so Navigate() is forced to create a
  // new compatible window. Because browser() supplied by the in-process
  // browser testing framework is compatible with browser()->profile(), we
  // need a different profile, and creating a popup window with an incognito
  // profile is a quick and dirty way of achieving this.
  Browser* popup = CreateEmptyBrowserForType(
      Browser::TYPE_POPUP,
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  NavigateParams params(MakeNavigateParams(popup));
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, params.browser);

  // This time, browser() is _not_ compatible with popup since it is not an
  // incognito window.
  EXPECT_NE(browser(), params.browser);

  // We should have three windows, each with one tab:
  // 1. the browser() provided by the framework (unchanged in this test)
  // 2. the incognito popup we created originally
  // 3. the new incognito tabbed browser that was created by Navigate().
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, popup->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
  EXPECT_TRUE(params.browser->is_type_normal());
  EXPECT_TRUE(params.browser->window()->IsToolbarVisible());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal Browser results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopup) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new, focused popup window, with a toolbar.
  EXPECT_NE(browser(), params.browser);
#if 0
  // TODO(stevenjb): Enable this test. See: crbug.com/79493
  EXPECT_TRUE(browser->window()->IsActive());
#endif
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->window()->IsToolbarVisible());

  // We should have two windows, the browser() provided by the framework and the
  // new popup window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a (kind of app) Browser results in a new Browser with TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopup_ExtensionId) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.app_id = "extensionappid";
  params.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new, focused TYPE_APP_POPUP window with no
  // toolbar.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_app_popup());
  EXPECT_FALSE(params.browser->window()->IsToolbarVisible());

  // We should have two windows, the browser() provided by the framework and the
  // new popup window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal popup results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupFromPopup) {
  // Open a popup.
  NavigateParams params1(MakeNavigateParams());
  params1.disposition = WindowOpenDisposition::NEW_POPUP;
  params1.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  Navigate(&params1);
  // Open another popup.
  NavigateParams params2(MakeNavigateParams(params1.browser));
  params2.disposition = WindowOpenDisposition::NEW_POPUP;
  params2.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  Navigate(&params2);

  // Navigate() should have opened a new normal popup window.
  EXPECT_NE(params1.browser, params2.browser);
  EXPECT_TRUE(params2.browser->is_type_popup());
  EXPECT_TRUE(params2.browser->window()->IsToolbarVisible());

  // We should have three windows, the browser() provided by the framework,
  // the first popup window, and the second popup window.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params1.browser->tab_strip_model()->count());
  EXPECT_EQ(1, params2.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app frame results in a new Browser with TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromAppWindow) {
  Browser* app_browser = CreateEmptyBrowserForApp(browser()->profile());
  NavigateParams params(MakeNavigateParams(app_browser));
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  Navigate(&params);

  // Navigate() should have opened a new TYPE_APP_POPUP window with no toolbar.
  EXPECT_NE(app_browser, params.browser);
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_app_popup());
  EXPECT_FALSE(params.browser->window()->IsToolbarVisible());

  // We should now have three windows, the app window, the app popup it created,
  // and the original browser() provided by the framework.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app popup results in a new Browser also of TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupFromAppPopup) {
  Browser* app_browser = CreateEmptyBrowserForApp(browser()->profile());
  // Open an app popup.
  NavigateParams params1(MakeNavigateParams(app_browser));
  params1.disposition = WindowOpenDisposition::NEW_POPUP;
  params1.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  Navigate(&params1);
  // Now open another app popup.
  NavigateParams params2(MakeNavigateParams(params1.browser));
  params2.disposition = WindowOpenDisposition::NEW_POPUP;
  params2.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  Navigate(&params2);

  // Navigate() should have opened a new popup app window.
  EXPECT_NE(browser(), params1.browser);
  EXPECT_NE(params1.browser, params2.browser);
  EXPECT_TRUE(params2.browser->is_type_app_popup());
  EXPECT_FALSE(params2.browser->window()->IsToolbarVisible());

  // We should now have four windows, the app window, the first app popup,
  // the second app popup, and the original browser() provided by the framework.
  EXPECT_EQ(4u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(1, params1.browser->tab_strip_model()->count());
  EXPECT_EQ(1, params2.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an extension app tab results in a new Browser with TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromExtensionApp) {
  // TODO(beng): TBD.
}

// This test verifies that navigating with window_action = SHOW_WINDOW_INACTIVE
// does not focus a new new popup window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupUnfocused) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  params.window_action = NavigateParams::SHOW_WINDOW_INACTIVE;
  // Wait for new popup to load (and gain focus if the test fails).
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new, unfocused, popup window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->window()->IsToolbarVisible());
#if 0
// TODO(stevenjb): Enable this test. See: crbug.com/79493
  EXPECT_FALSE(p.browser->window()->IsActive());
#endif
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// and trusted_source = true results in a new Browser where is_trusted_source()
// is true.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupTrusted) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.trusted_source = true;
  params.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new popup window of TYPE_POPUP with no
  // toolbar.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->is_trusted_source());
  EXPECT_FALSE(params.browser->window()->IsToolbarVisible());
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// and NavigateParams::CaptivePortalWindowType::kPopup results in a new
// WebContents where is_captive_portal_window() is true.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupCaptivePortal) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.captive_portal_window_type =
      captive_portal::CaptivePortalWindowType::kPopup;
  params.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new popup window of TYPE_POPUP with a
  // toolbar.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->window()->IsToolbarVisible());
  EXPECT_TRUE(captive_portal::CaptivePortalTabHelper::FromWebContents(
                  params.navigated_or_inserted_contents)
                  ->is_captive_portal_window());
}
#endif

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// and is_tab_modal_popup = true results in a new WebContents that is a popup
// and behaves like a tab modal.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupTabModal) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.is_tab_modal_popup = true;
  params.window_features.bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Add a new tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  // Switch to the new tab.
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Verify the popup window is hidden.
  EXPECT_FALSE(params.browser->window()->IsVisible());

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Verify the popup window is visible again.
  EXPECT_TRUE(params.browser->window()->IsVisible());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_WINDOW
// always opens a new window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewWindow) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  Navigate(&params);

  // Navigate() should have opened a new toplevel window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_normal());
  EXPECT_TRUE(params.browser->window()->IsToolbarVisible());

  // We should now have two windows, the browser() provided by the framework and
  // the new normal window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that a source tab to the left of the target tab can
// be switched away from and closed. It verifies that if we close the
// earlier tab, that we don't use a stale index, and select the wrong tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, OutOfOrderTabSwitchTest) {
  const GURL singleton_url("http://maps.google.com/");

  NavigateHelper(singleton_url, browser(),
                 WindowOpenDisposition::NEW_FOREGROUND_TAB, true);
  WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();

  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  NavigateHelper(singleton_url, browser(), WindowOpenDisposition::SWITCH_TO_TAB,
                 false, new_tab);
}

// This test verifies the two cases of attempting to switch to a tab that no
// longer exists: if NTP, load in current tab, otherwise load in new
// foreground tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NavigateOnTabSwitchLostTest) {
  const GURL singleton_url("chrome://dino");

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateHelper(singleton_url, browser(), WindowOpenDisposition::SWITCH_TO_TAB,
                 true, tab);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  NavigateHelper(GURL("chrome://about"), browser(),
                 WindowOpenDisposition::NEW_FOREGROUND_TAB, true);
  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
  // This expects a new WebContents, since we just closed the tab.
  NavigateHelper(singleton_url, browser(), WindowOpenDisposition::SWITCH_TO_TAB,
                 true, nullptr /* expected_contents */);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This test verifies that SWITCH_TO_TAB will switch to a tab even if the scheme
// mismatches, as long as the rest of the URL does.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, SchemeMismatchTabSwitchTest) {
  GURL navigate_url("https://www.chromium.org/");
  GURL search_url("http://www.chromium.org/");
  GURL dino_url("chrome://dino");

  NavigateHelper(navigate_url, browser(), WindowOpenDisposition::CURRENT_TAB,
                 true);
  NavigateHelper(dino_url, browser(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
                 true);

  // We must be on another tab than the target for it to be found and
  // switched to. To meet that requirement, ensure the dino tab is currently
  // active.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  NavigateHelper(search_url, browser(), WindowOpenDisposition::SWITCH_TO_TAB,
                 false);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Make sure that switching tabs preserves the post-focus state (of the
// content area) of the previous tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, SaveAfterFocusTabSwitchTest) {
  GURL first_url("chrome://dino/");
  GURL second_url("chrome://history/");

  NavigateHelper(first_url, browser(), WindowOpenDisposition::CURRENT_TAB,
                 true);

  // Generate history so the tab isn't closed.
  NavigateHelper(second_url, browser(),
                 WindowOpenDisposition::NEW_FOREGROUND_TAB, true);

  LocationBar* location_bar = browser()->window()->GetLocationBar();
  location_bar->FocusLocation(true);

  NavigateHelper(first_url, browser(), WindowOpenDisposition::SWITCH_TO_TAB,
                 false);

  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  EXPECT_EQ(omnibox_view->model()->focus_state(),
            OmniboxFocusState::OMNIBOX_FOCUS_NONE);
}

// This test verifies that we're picking the correct browser and tab to
// switch to. It verifies that we don't recommend the active tab, and that,
// when switching, we don't mistakenly pick the current browser. Note that this
// test checks which window the new tab was created in, but does not check
// whether the target window was activated - that would require a much slower
// interactive UI test, since we'd have to wait for the async window activation
// to complete to avoid flakes.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, SwitchToTabCorrectWindow) {
  const GURL url1("http://example1.chromium.org");
  const GURL url2("http://example2.chromium.org");

  // Make singleton tab.
  Browser* browser1 =
      NavigateHelper(url1, browser(), WindowOpenDisposition::CURRENT_TAB, true);

  // Make a new window with different URL.
  Browser* browser2 =
      NavigateHelper(url2, browser1, WindowOpenDisposition::NEW_WINDOW, true);
  EXPECT_NE(browser1, browser2);

  EXPECT_EQ(browser1,
            NavigateHelper(url1, browser2, WindowOpenDisposition::SWITCH_TO_TAB,
                           false));
  EXPECT_EQ(browser2,
            NavigateHelper(url2, browser1, WindowOpenDisposition::SWITCH_TO_TAB,
                           false));
}

// TODO(crbug.com/40806044): Reactivate the test.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// This test verifies that "switch to tab" prefers the latest used browser,
// if multiple exist.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, DISABLED_SwitchToTabLatestWindow) {
  // Navigate to a site.
  NavigateHelper(GURL("http://maps.google.com/"), browser(),
                 WindowOpenDisposition::CURRENT_TAB, true);

  // Navigate to a new window.
  Browser* browser1 = NavigateHelper(GURL("http://maps.google.com/"), browser(),
                                     WindowOpenDisposition::NEW_WINDOW, true);

  // Make yet another window.
  Browser* browser2 = NavigateHelper(GURL("http://maps.google.com/"), browser(),
                                     WindowOpenDisposition::NEW_WINDOW, true);

  // Navigate to the latest copy of the URL, in spite of specifying
  // the previous browser.
  Browser* test_browser =
      NavigateHelper(GURL("http://maps.google.com/"), browser1,
                     WindowOpenDisposition::SWITCH_TO_TAB, false);

  EXPECT_EQ(browser2, test_browser);
}
#endif

// Tests that a disposition of SINGLETON_TAB cannot see outside its
// window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, SingletonWindowLeak) {
  // Navigate to a site.
  NavigateHelper(GURL("chrome://dino"), browser(),
                 WindowOpenDisposition::CURRENT_TAB, true);

  // Navigate to a new window.
  Browser* browser2 = NavigateHelper(GURL("chrome://about"), browser(),
                                     WindowOpenDisposition::NEW_WINDOW, true);

  // Make sure we open non-special URL here.
  Browser* test_browser =
      NavigateHelper(GURL("chrome://dino"), browser2,
                     WindowOpenDisposition::NEW_FOREGROUND_TAB, true);
  EXPECT_EQ(browser2, test_browser);
}

// Tests that a disposition of SINGLETON_TAB cannot see across anonymity,
// except for certain non-incognito affinity URLs (e.g. settings).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, SingletonIncognitoLeak) {
  Browser* orig_browser;

  // Navigate to a site.
  orig_browser = NavigateHelper(GURL(chrome::kChromeUIVersionURL), browser(),
                                WindowOpenDisposition::CURRENT_TAB, true);

  // Open about for (not) finding later.
  NavigateHelper(GURL(chrome::kChromeUIAboutURL), orig_browser,
                 WindowOpenDisposition::NEW_FOREGROUND_TAB, true);

  // Also open settings for finding later.
  NavigateHelper(GURL(chrome::kChromeUISettingsURL), orig_browser,
                 WindowOpenDisposition::NEW_FOREGROUND_TAB, false);

  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  Browser* test_browser;

  {
    Browser* incognito_browser = CreateIncognitoBrowser();

    test_browser =
        NavigateHelper(GURL(chrome::kChromeUIDownloadsURL), incognito_browser,
                       WindowOpenDisposition::OFF_THE_RECORD, true);
    // Sanity check where OTR tab landed.
    EXPECT_EQ(incognito_browser, test_browser);

    // Sanity check that browser() always returns original.
    EXPECT_EQ(orig_browser, browser());

    // Open about singleton. Should not find in regular browser and
    // open locally.
    test_browser =
        NavigateHelper(GURL(chrome::kChromeUIAboutURL), incognito_browser,
                       WindowOpenDisposition::SINGLETON_TAB, true);
    EXPECT_NE(orig_browser, test_browser);

    // Open settings. Should switch to non-incognito profile to do so.
    test_browser =
        NavigateHelper(GURL(chrome::kChromeUISettingsURL), incognito_browser,
                       WindowOpenDisposition::SINGLETON_TAB, false);
    EXPECT_EQ(orig_browser, test_browser);
  }

  // Open downloads singleton. Should not search OTR browser and
  // should open in regular browser.
  test_browser =
      NavigateHelper(GURL(chrome::kChromeUIDownloadsURL), orig_browser,
                     WindowOpenDisposition::SINGLETON_TAB, true);
  EXPECT_EQ(browser(), test_browser);
}

// Tests that a disposition of SWITCH_TAB cannot see across anonymity,
// except for certain non-incognito affinity URLs (e.g. settings).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, SwitchToTabIncognitoLeak) {
  Browser* orig_browser;

  // Navigate to a site.
  orig_browser = NavigateHelper(GURL(chrome::kChromeUIVersionURL), browser(),
                                WindowOpenDisposition::CURRENT_TAB, true);

  // Also open settings for finding later.
  NavigateHelper(GURL(chrome::kChromeUISettingsURL), orig_browser,
                 WindowOpenDisposition::NEW_FOREGROUND_TAB, false);

  // Also open about for searching too.
  NavigateHelper(GURL(chrome::kChromeUIAboutURL), orig_browser,
                 WindowOpenDisposition::NEW_FOREGROUND_TAB, true);

  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  Browser* test_browser;

  {
    Browser* incognito_browser = CreateIncognitoBrowser();

    test_browser =
        NavigateHelper(GURL(chrome::kChromeUIDownloadsURL), incognito_browser,
                       WindowOpenDisposition::OFF_THE_RECORD, true);
    // Sanity check where OTR tab landed.
    EXPECT_EQ(incognito_browser, test_browser);

    // Sanity check that browser() always returns original.
    EXPECT_EQ(orig_browser, browser());

    // Try to open the original chrome://about via switch-to-tab. Should not
    // find copy in regular browser, and open new tab in incognito.
    test_browser =
        NavigateHelper(GURL(chrome::kChromeUIAboutURL), incognito_browser,
                       WindowOpenDisposition::SWITCH_TO_TAB, true);
    EXPECT_EQ(incognito_browser, test_browser);

    // Open settings. Should switch to non-incognito profile to do so.
    test_browser =
        NavigateHelper(GURL(chrome::kChromeUISettingsURL), incognito_browser,
                       WindowOpenDisposition::SWITCH_TO_TAB, false);
    EXPECT_EQ(orig_browser, test_browser);
  }

  // Switch-to-tab shouldn't find the incognito tab, and open new one in
  // current browser.
  test_browser =
      NavigateHelper(GURL(chrome::kChromeUIDownloadsURL), orig_browser,
                     WindowOpenDisposition::SWITCH_TO_TAB, true);
  EXPECT_EQ(browser(), test_browser);
}

#if BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Mac. See https://crbug.com/674497.
#define MAYBE_Disposition_Incognito DISABLED_Disposition_Incognito
#else
#define MAYBE_Disposition_Incognito Disposition_Incognito
#endif
// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// opens a new incognito window if no existing incognito window is present.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, MAYBE_Disposition_Incognito) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  Navigate(&params);

  // Navigate() should have opened a new toplevel incognito window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_EQ(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      params.browser->profile());

  // |source_contents| should be set to NULL because the profile for the new
  // page is different from the originating page.
  EXPECT_FALSE(params.source_contents);

  // We should now have two windows, the browser() provided by the framework and
  // the new incognito window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// reuses an existing incognito window when possible.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IncognitoRefocus) {
  Browser* incognito_browser = CreateEmptyBrowserForType(
      Browser::TYPE_NORMAL,
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  Navigate(&params);

  // Navigate() should have opened a new tab in the existing incognito window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_EQ(params.browser, incognito_browser);

  // We should now have two windows, the browser() provided by the framework and
  // the incognito window we opened earlier.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = SAVE_TO_DISK.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SaveToDisk) {
  RunSuppressTest(WindowOpenDisposition::SAVE_TO_DISK);
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = IGNORE_ACTION.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IgnoreAction) {
  RunSuppressTest(WindowOpenDisposition::IGNORE_ACTION);
}

// This tests adding a foreground tab with a predefined WebContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, TargetContents_ForegroundTab) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.contents_to_insert = CreateWebContents(false);
  Navigate(&params);

  // Navigate() should have opened the contents in a new foreground tab in the
  // current Browser.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            params.navigated_or_inserted_contents);

  // We should have one window, with two tabs.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

#if BUILDFLAG(IS_WIN)
// This tests adding a popup with a predefined WebContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, DISABLED_TargetContents_Popup) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = CreateWebContents(false);
  params.window_features.bounds = gfx::Rect(10, 10, 500, 500);
  Navigate(&params);

  // Navigate() should have opened a new popup window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->window()->IsToolbarVisible());

  // The web platform is weird. The window bounds specified in
  // `params.window_features.bounds` are used as follows:
  // - the origin is used to position the window
  // - the size is used to size the WebContents of the window.
  // As such the position of the resulting window will always match
  // `params.window_features.bounds.origin()`, but its size will not. We need to
  // match the size against the selected tab's view's container size.
  // Only Windows positions the window according to
  // `params.window_features.bounds.origin()` - on Mac the window is offset from
  // the opener and on Linux it always opens at 0,0.
  EXPECT_EQ(params.window_features.bounds.origin(),
            params.browser->window()->GetRestoredBounds().origin());
  // All platforms should respect size however provided width > 400 (Mac has a
  // minimum window width of 400).
  EXPECT_EQ(params.window_features.bounds.size(),
            params.navigated_or_inserted_contents->GetContainerBounds().size());

  // We should have two windows, the new popup and the browser() provided by the
  // framework.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}
#endif

// This test checks that we can create WebContents with renderer process and
// RenderFrame without navigating it.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       CreateWebContentsWithRendererProcess) {
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.contents_to_insert = CreateWebContents(true);
  ASSERT_TRUE(params.contents_to_insert);

  // There is no navigation (to about:blank or something like that).
  EXPECT_FALSE(params.contents_to_insert->IsLoading());

  ASSERT_TRUE(params.contents_to_insert->GetPrimaryMainFrame());
  EXPECT_TRUE(
      params.contents_to_insert->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(
      params.contents_to_insert->GetController().IsInitialBlankNavigation());
  int renderer_id =
      params.contents_to_insert->GetPrimaryMainFrame()->GetProcess()->GetID();

  // We should have one window, with one tab of WebContents differ from
  // params.target_contents.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(browser()->tab_strip_model()->GetActiveWebContents(),
            params.contents_to_insert.get());

  Navigate(&params);

  // Navigate() should have opened the contents in a new foreground tab in the
  // current Browser, without changing the renderer process of target_contents.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            params.navigated_or_inserted_contents);
  EXPECT_EQ(renderer_id,
            params.navigated_or_inserted_contents->GetPrimaryMainFrame()
                ->GetProcess()
                ->GetID());

  // We should have one window, with two tabs.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This tests adding a tab at a specific index.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Tabstrip_InsertAtIndex) {
  // This is not meant to be a comprehensive test of whether or not the tab
  // implementation of the browser observes the insertion index. That is
  // covered by the unit tests for TabStripModel. This merely verifies that
  // insertion index preference is reflected in common cases.
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.tabstrip_add_types = AddTabTypes::ADD_FORCE_INDEX;
  Navigate(&params);

  // Navigate() should have inserted a new tab at slot 0 in the tabstrip.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(
                   static_cast<const WebContents*>(
                       params.navigated_or_inserted_contents)));

  // We should have one window - the browser() provided by the framework.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens a new tab navigated to the specified URL if
// no previous tab with that URL (minus the path) exists.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNew_IgnorePath) {
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate to a new singleton tab with a sub-page.
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GetContentSettingsURL();
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  // The last tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GetContentSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnorePath) {
  const GURL singleton_url(GetSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url,
                                ui::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  // Navigate to |singleton_url|.
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GetContentSettingsURL();
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GetContentSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath) {
  const GURL singleton_url(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url,
                                ui::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  // Navigate to |singleton_url|.
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GetClearBrowsingDataURL();
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will update the current tab's URL if the currently
// selected tab is a match but has a different path.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabFocused_IgnorePath) {
  const GURL singleton_url_current(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url_current,
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate to a different settings path.
  const GURL singleton_url_target(GetClearBrowsingDataURL());
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url_target;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  // The second tab should still be selected, but navigated to the new path.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(singleton_url_target,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will open an existing matching tab with a different
// query.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnoreQuery) {
  int initial_tab_count = browser()->tab_strip_model()->count();
  const GURL singleton_url_current(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url_current,
                                ui::PAGE_TRANSITION_LINK);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(initial_tab_count, browser()->tab_strip_model()->active_index());

  // Navigate to a different settings path.
  const GURL singleton_url_target(GetClearBrowsingDataURL());
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url_target;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  // Last tab should still be selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(initial_tab_count, browser()->tab_strip_model()->active_index());
}

// This test verifies that the settings page isn't opened in the incognito
// window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Settings_UseNonIncognitoWindow) {
  RunUseNonIncognitoWindowTest(
      GetSettingsURL(), ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK);
}

// This test verifies that the view-source settings page isn't opened in the
// incognito window.
IN_PROC_BROWSER_TEST_F(
    BrowserNavigatorTest,
    Disposition_ViewSource_Settings_DoNothingIfIncognitoForced) {
  std::string view_source(content::kViewSourceScheme);
  view_source.append(":");
  view_source.append(chrome::kChromeUISettingsURL);
  RunDoNothingIfIncognitoIsForcedTest(GURL(view_source));
}

// This test verifies that the view-source settings page isn't opened in the
// incognito window even if incognito mode is forced (does nothing in that
// case).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_ViewSource_Settings_UseNonIncognitoWindow) {
  std::string view_source(content::kViewSourceScheme);
  view_source.append(":");
  view_source.append(chrome::kChromeUISettingsURL);
  RunUseNonIncognitoWindowTest(
      GURL(view_source), ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK);
}

// This test verifies that the settings page isn't opened in the incognito
// window from a non-incognito window (bookmark open-in-incognito trigger).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Settings_UseNonIncognitoWindowForBookmark) {
  NavigateParams params(browser(), GetSettingsURL(),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  {
    content::CreateAndLoadWebContentsObserver observer;
    Navigate(&params);
    observer.Wait();
  }

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Settings page is expected to always open in normal mode regardless
// of whether the user is trying to open it in incognito mode or not.
// This test verifies that if incognito mode is forced (by policy), settings
// page doesn't open at all.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Settings_DoNothingIfIncognitoIsForced) {
  RunDoNothingIfIncognitoIsForcedTest(GetSettingsURL());
}

// This test verifies that the bookmarks page isn't opened in the incognito
// window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Bookmarks_UseNonIncognitoWindow) {
  RunUseNonIncognitoWindowTest(
      GURL(chrome::kChromeUIBookmarksURL),
      ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK);
}

// Bookmark manager is expected to always open in normal mode regardless
// of whether the user is trying to open it in incognito mode or not.
// This test verifies that if incognito mode is forced (by policy), bookmark
// manager doesn't open at all.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Bookmarks_DoNothingIfIncognitoIsForced) {
  RunDoNothingIfIncognitoIsForcedTest(GURL(chrome::kChromeUIBookmarksURL));
}

// This test makes sure a crashed singleton tab reloads from a new navigation.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NavigateToCrashedSingletonTab) {
  const GURL singleton_url(GetContentSettingsURL());
  WebContents* web_contents = chrome::AddSelectedTabWithURL(
      browser(), singleton_url, ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Kill the singleton tab.
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

    content::RenderFrameDeletedObserver crash_observer(
        web_contents->GetPrimaryMainFrame());
    web_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(web_contents->IsCrashed());

  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ui_test_utils::NavigateToURL(&params);

  // The tab should not be sad anymore.
  EXPECT_FALSE(web_contents->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToOptionsInSameTab) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// TODO(crbug.com/40107334): Timing out on linux-chromeos-dbg.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_NavigateFromBlankToOptionsInSameTab \
  DISABLED_NavigateFromBlankToOptionsInSameTab
#else
#define MAYBE_NavigateFromBlankToOptionsInSameTab \
  NavigateFromBlankToOptionsInSameTab
#endif
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       MAYBE_NavigateFromBlankToOptionsInSameTab) {
  NavigateParams params(MakeNavigateParams());
  params.url = GURL(url::kAboutBlankURL);
  ui_test_utils::NavigateToURL(&params);

  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// TODO(crbug.com/40107334): Timing out on linux-chromeos-dbg.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_NavigateFromNTPToOptionsInSameTab \
  DISABLED_NavigateFromNTPToOptionsInSameTab
#else
#define MAYBE_NavigateFromNTPToOptionsInSameTab \
  NavigateFromNTPToOptionsInSameTab
#endif
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       MAYBE_NavigateFromNTPToOptionsInSameTab) {
  NavigateParams params(MakeNavigateParams());
  params.url = GURL(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(browser()->profile()),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());

  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromPageToOptionsInNewTab) {
  NavigateParams params(MakeNavigateParams());
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(GetGoogleURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  {
    content::CreateAndLoadWebContentsObserver observer;
    ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsSingleton) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// TODO(crbug.com/40166082): This is disabled for Mac OS due to flakiness.
// TODO(crbug.com/40107334): Timing out on linux-chromeos-dbg.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_NavigateFromNTPToOptionsPageInSameTab \
  DISABLED_NavigateFromNTPToOptionsPageInSameTab
#else
#define MAYBE_NavigateFromNTPToOptionsPageInSameTab \
  NavigateFromNTPToOptionsPageInSameTab
#endif
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       MAYBE_NavigateFromNTPToOptionsPageInSameTab) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::ShowSettingsSubPageInTabbedBrowser(
        browser(), chrome::kClearBrowserDataSubPage);
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::ShowSettingsSubPageInTabbedBrowser(
        browser(), chrome::kClearBrowserDataSubPage);
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromPageInfoToSiteSettingsInNewTab) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromePageInfoDelegate delegate(web_contents);
  delegate.ShowSiteSettings(web_contents->GetVisibleURL());
  content::LoadStopObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  observer.Wait();

  // Site settings opens in a new tab.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(chrome::GetSettingsUrl(chrome::kContentSettingsSubPage),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromPageInfoToSiteSettingsFileSystemInNewTab) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromePageInfoDelegate delegate(web_contents);
  delegate.OpenContentSettingsExceptions(
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  content::LoadStopObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  observer.Wait();

  // File system site settings opens in a new tab.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(chrome::GetSettingsUrl(chrome::kFileSystemSettingsSubpage),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromOtherTabToSingletonOptions) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }
  {
    content::CreateAndLoadWebContentsObserver observer;
    chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  // This load should simply cause a tab switch.
  ShowSettings(browser());

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNoTabStripWindowToOptions) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }
  {
    content::CreateAndLoadWebContentsObserver observer;
    chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
  Browser* app_browser = CreateBrowserForApp("TestApp", browser()->profile());

  // This load should cause a window and tab switch.
  ShowSingletonTab(app_browser, GetSettingsURL());

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// TODO(crbug.com/40107334): Timing out on linux-chromeos-dbg.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_CloseSingletonTab DISABLED_CloseSingletonTab
#else
#define MAYBE_CloseSingletonTab CloseSingletonTab
#endif
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, MAYBE_CloseSingletonTab) {
  for (int i = 0; i < 2; ++i) {
    content::CreateAndLoadWebContentsObserver observer;
    chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                  ui::PAGE_TRANSITION_TYPED);
    observer.Wait();
  }

  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ShowSettings(browser());
    observer.Wait();
  }

  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(
      2, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToHistoryInSameTab) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::ShowHistory(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToBookmarksInSameTab) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::ShowBookmarkManager(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(base::StartsWith(
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec(),
      chrome::kChromeUIBookmarksURL, base::CompareCase::SENSITIVE));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToDownloadsInSameTab) {
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::ShowDownloads(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NavigateWithoutBrowser) {
  // First navigate using the profile of the existing browser window, and
  // check that the window is reused.
  NavigateParams params(browser()->profile(), GetGoogleURL(),
                        ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Now navigate using the incognito profile and check that a new window
  // is created.
  NavigateParams params_incognito(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      GetGoogleURL(), ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params_incognito);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, ViewSourceIsntSingleton) {
  const std::string viewsource_ntp_url =
      std::string(content::kViewSourceScheme) + ":" +
      chrome::kChromeUIVersionURL;

  NavigateParams viewsource_params(browser(), GURL(viewsource_ntp_url),
                                   ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&viewsource_params);

  NavigateParams singleton_params(browser(), GURL(chrome::kChromeUIVersionURL),
                                  ui::PAGE_TRANSITION_LINK);
  singleton_params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  EXPECT_EQ(-1, GetIndexOfExistingTab(browser(), singleton_params));
}

// Ensure that an incognito window invoking |view-source:| on a url forbidden in
// incognito loads the correct url in the non-incognito window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, ViewSourceUrlMatching) {
  // Open chrome://settings in the main window.
  NavigateParams settings_params(browser(), GURL(chrome::kChromeUISettingsURL),
                                 ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&settings_params);

  // Create a new incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());

  // In the Incognito window, start a navigation to the view-source page.
  const std::string viewsource_settings_url =
      std::string(content::kViewSourceScheme) + ":" +
      chrome::kChromeUISettingsURL;
  NavigateParams params(MakeNavigateParams(incognito_browser));
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GURL(viewsource_settings_url);
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  Navigate(&params);

  // The view-source page should be opened as a new tab in the non-incognito
  // browser window.
  EXPECT_NE(incognito_browser, params.browser);
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(viewsource_settings_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

enum class SplitCacheTestCase {
  kEnabledTripleKeyed,
  kEnabledTriplePlusCrossSiteMainFrameNavBool,
  kEnabledTriplePlusMainFrameNavInitiator,
  kEnabledTriplePlusNavInitiator
};
const struct {
  const SplitCacheTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
     net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
    {SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
     net::features::kSplitCacheByMainFrameNavigationInitiator},
    {SplitCacheTestCase::kEnabledTriplePlusNavInitiator,
     net::features::kSplitCacheByNavigationInitiator}};

class BrowserNavigatorSplitHttpCacheTest
    : public BrowserNavigatorTest,
      public testing::WithParamInterface<SplitCacheTestCase> {
 protected:
  BrowserNavigatorSplitHttpCacheTest()
      : split_cache_experiment_feature_list_(GetParam(),
                                             kTestCaseToFeatureMapping) {
    split_cache_always_enabled_feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
  base::test::ScopedFeatureList split_cache_always_enabled_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserNavigatorSplitHttpCacheTest,
    testing::ValuesIn(
        {SplitCacheTestCase::kEnabledTripleKeyed,
         SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
         SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
         SplitCacheTestCase::kEnabledTriplePlusNavInitiator}),
    [](const testing::TestParamInfo<SplitCacheTestCase>& info) {
      switch (info.param) {
        case (SplitCacheTestCase::kEnabledTripleKeyed):
          return "TripleKeyed";
        case (SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool):
          return "TriplePlusCrossSiteMainFrameNavigationBool";
        case (SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator):
          return "TriplePlusMainFrameNavigationInitiator";
        case (SplitCacheTestCase::kEnabledTriplePlusNavInitiator):
          return "TriplePlusNavigationInitiator";
      }
    });

// This test verifies that browser initiated navigations can send requests
// using POST.
IN_PROC_BROWSER_TEST_P(BrowserNavigatorSplitHttpCacheTest,
                       SendBrowserInitiatedRequestUsingPOST) {
  // Uses a test sever to verify POST request.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open a browser initiated POST request in new foreground tab.
  std::u16string expected_title(kExpectedTitle16);
  std::string post_data = kExpectedTitle;
  std::u16string title;
  ASSERT_TRUE(OpenPOSTURLInNewForegroundTabAndGetTitle(
      embedded_test_server()->GetURL(kEchoTitleCommand), post_data, true,
      &title));
  EXPECT_EQ(expected_title, title);
}

// This test verifies that renderer initiated navigations can also send requests
// using POST.
IN_PROC_BROWSER_TEST_P(BrowserNavigatorSplitHttpCacheTest,
                       SendRendererInitiatedRequestUsingPOST) {
  // Uses a test sever to verify POST request.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open a renderer initiated POST request in new foreground tab.
  std::u16string expected_title(kExpectedTitle16);
  std::string post_data = kExpectedTitle;
  std::u16string title;
  ASSERT_TRUE(OpenPOSTURLInNewForegroundTabAndGetTitle(
      embedded_test_server()->GetURL(kEchoTitleCommand), post_data, false,
      &title));
  EXPECT_EQ(expected_title, title);
}

// This test navigates to a data URL that contains BiDi control
// characters. For security reasons, BiDi control chars should always be
// escaped in the URL but they should be unescaped in the loaded HTML.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateToDataURLWithBiDiControlChars) {
  // Text in Arabic.
  std::string text = "\xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1";
  // Page title starts with RTL mark.
  std::string unescaped_title = "\xE2\x80\x8F" + text;
  std::string data_url = "data:text/html;charset=utf-8,<html><title>" +
                         unescaped_title + "</title></html>";
  // BiDi control chars in URLs are always escaped, so the expected URL should
  // have the title with the escaped RTL mark.
  std::string escaped_title = "%E2%80%8F" + text;
  std::string expected_url = "data:text/html;charset=utf-8,<html><title>" +
                             escaped_title + "</title></html>";

  // Navigate to the page.
  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.url = GURL(data_url);
  params.window_action = NavigateParams::SHOW_WINDOW;
  ui_test_utils::NavigateToURL(&params);

  std::u16string expected_title(base::UTF8ToUTF16(unescaped_title));
  EXPECT_TRUE(params.navigated_or_inserted_contents);
  EXPECT_EQ(expected_title, params.navigated_or_inserted_contents->GetTitle());
  // GURL always keeps non-ASCII characters escaped, but check them anyways.
  EXPECT_EQ(GURL(expected_url).spec(),
            params.navigated_or_inserted_contents->GetURL().spec());
  // Check the omnibox text. It should have escaped RTL with unescaped text.
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  EXPECT_EQ(base::UTF8ToUTF16(expected_url), omnibox_view->GetText());
}

// Test that main frame navigations generate a NavigationUIData with the
// correct disposition.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, MainFrameNavigationUIData) {
  ASSERT_TRUE(embedded_test_server()->Start());

  {
    const GURL url = embedded_test_server()->GetURL("/title1.html");
    TestNavigationUIDataObserver observer(url);

    NavigateParams params(MakeNavigateParams());
    params.url = url;
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
    observer.WaitForNavigationFinished();

    EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
              observer.last_navigation_ui_data()->window_open_disposition());
  }

  {
    const GURL url = embedded_test_server()->GetURL("/title2.html");
    TestNavigationUIDataObserver observer(url);

    NavigateParams params(MakeNavigateParams());
    params.url = url;
    params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
    observer.WaitForNavigationFinished();

    EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
              observer.last_navigation_ui_data()->window_open_disposition());
  }
}

// TODO(crbug.com/40806044): Reactivate the test.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Test that subframe navigations generate a NavigationUIData with no
// disposition.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, SubFrameNavigationUIData) {
  ASSERT_TRUE(embedded_test_server()->Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Load page with iframe.
  const GURL url1 = embedded_test_server()->GetURL("/iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Retrieve the iframe.
  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(iframe);

  // Navigate the iframe with a disposition.
  NavigateParams params(browser(),
                        embedded_test_server()->GetURL("/simple.html"),
                        ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;

  TestNavigationUIDataObserver observer(
      embedded_test_server()->GetURL("/simple.html"));
  ui_test_utils::NavigateToURL(&params);
  observer.WaitForNavigationFinished();

  // The disposition passed to NavigateToURL should be ignored for sub frame
  // navigations.
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            observer.last_navigation_ui_data()->window_open_disposition());
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_PictureInPicture_Open) {
  // Create the params for the PiP request.
  auto pip_options = blink::mojom::PictureInPictureWindowOptions::New();

  // The WebContents holds the parameters from the PiP request.
  WebContents::CreateParams web_contents_params(browser()->profile());
  web_contents_params.picture_in_picture_options = *pip_options;

  // Opening a picture in picture window should create a new browser.
  NavigateParams params = MakeNavigateParams(browser());
  params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;

  // Navigate to https:// page
  // TODO: Extract the navigation logic to a helper function?
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  const GURL url = https_server.GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  params.source_contents = tab;
  params.contents_to_insert = WebContents::Create(web_contents_params);
  Navigate(&params);

  // Should not re-use the browser.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_picture_in_picture());
  EXPECT_EQ(params.browser->app_name(), std::string());

  // The window should have respected the initial aspect ratio.
  const gfx::Rect override_bounds = params.browser->override_bounds();
  const double aspect_ratio = static_cast<double>(override_bounds.width()) /
                              static_cast<double>(override_bounds.height());
  EXPECT_DOUBLE_EQ(1.0, aspect_ratio);
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_PictureInPicture_OpenWithWidthAndHeight) {
  // Set width/height with equivalent aspect ratio of 1.0.
  auto pip_options = blink::mojom::PictureInPictureWindowOptions::New();
  pip_options->width = 500;
  pip_options->height = 500;
  WebContents::CreateParams web_contents_params(browser()->profile());
  web_contents_params.picture_in_picture_options = *pip_options;

  // Opening a picture in picture window should create a new browser.
  NavigateParams params = MakeNavigateParams(browser());
  params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;

  // Navigate to https:// page
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  const GURL url = https_server.GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  params.source_contents = tab;
  params.contents_to_insert = WebContents::Create(web_contents_params);
  Navigate(&params);

  // The bounds may have small adjustments for window decorations, since the
  // requested size is the inner size.  We can't get the inner size easily here,
  // so just verify that the aspect ratio is closer to 1.0 than 0.5.
  const gfx::Rect override_bounds = params.browser->override_bounds();
  float expected_aspect_ratio =
      static_cast<float>(override_bounds.width()) / override_bounds.height();
  EXPECT_NEAR(expected_aspect_ratio, 1.0f, 0.2);
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_PictureInPicture_CantFromAnotherPip) {
  // Make sure that attempting to open a picture in picture window from a
  // picture in picture window fails.
  Browser* pip = CreateEmptyBrowserForType(Browser::TYPE_PICTURE_IN_PICTURE,
                                           browser()->profile());
  NavigateParams params = MakeNavigateParams(pip);
  params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;

  // Navigate to https:// page
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  const GURL url = https_server.GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  params.source_contents = tab;
  Navigate(&params);

  EXPECT_EQ(params.browser, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    BrowserNavigatorTest,
    Disposition_PictureInPicture_CantWithoutASourceContents) {
  // Opening a picture-in-picture window without a source contents should fail.
  Browser* pip = CreateEmptyBrowserForType(Browser::TYPE_PICTURE_IN_PICTURE,
                                           browser()->profile());
  NavigateParams params = MakeNavigateParams(pip);
  params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;
  params.source_contents = nullptr;

  EXPECT_EQ(nullptr, Navigate(&params));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_PictureInPicture_CantFromAboutBlank) {
  // Disallow document PiP windows from opening from a window with about:blank
  // in the omnibox
  Browser* pip = CreateEmptyBrowserForType(Browser::TYPE_PICTURE_IN_PICTURE,
                                           browser()->profile());
  NavigateParams params = MakeNavigateParams(pip);
  params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(tab->GetLastCommittedURL().IsAboutBlank());
  params.source_contents = tab;
  EXPECT_EQ(nullptr, Navigate(&params));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_PictureInPicture_OpenFromWebApp) {
  // Create the params for the PiP request that looks like it's from an app.
  auto pip_options = blink::mojom::PictureInPictureWindowOptions::New();

  // The WebContents holds the parameters from the PiP request.
  WebContents::CreateParams web_contents_params(browser()->profile());
  web_contents_params.picture_in_picture_options = *pip_options;

  // Opening a picture in picture window should create a new browser.
  NavigateParams params = MakeNavigateParams(browser());
  params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;
  params.app_id = "extensionappid";

  // Navigate to https:// page
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  const GURL url = https_server.GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  params.source_contents = tab;
  params.contents_to_insert = WebContents::Create(web_contents_params);
  Navigate(&params);

  // Should be PiP, with an app name.
  EXPECT_TRUE(params.browser->is_type_picture_in_picture());
  EXPECT_NE(params.browser->app_name(), std::string());
}

// Test typical popin UX flow.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Popin) {
  // Setup server.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate to a.test.
  const GURL url = embedded_https_test_server().GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);

  // Open popin and verify it's visible.
  content::WebContentsAddedObserver new_tab_observer;
  EXPECT_TRUE(content::ExecJs(
      tab_web_contents,
      "window.open('" + url.spec() + "?popin_policy=*', '_blank', 'popin')"));
  content::WebContents* popin_web_contents = new_tab_observer.GetWebContents();
  BrowserWindow* popin_browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(popin_web_contents);
  EXPECT_NE(popin_browser_window, browser()->window());
  EXPECT_TRUE(popin_browser_window->IsVisible());

  // Focus new tab and verify popin is hidden.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(popin_browser_window->IsVisible());

  // Switch back to original tab and verify popin is visible.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(popin_browser_window->IsVisible());
}

// Verify that a popin must not be insecure nor opened from an insecure context.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, PopinContext) {
  // Setup servers.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_https_test_server().Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a.test.
  const GURL secure_url =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL insecure_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Insecure context and insecure popin fails.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), insecure_url));
  auto result = content::ExecJs(tab_web_contents,
                                "window.open('" + insecure_url.spec() +
                                    "?popin_policy=*', '_blank', 'popin')");
  EXPECT_TRUE(strstr(result.message(),
                     "Partitioned popins must be opened from https URLs."));

  // Secure context and insecure popin fails.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), secure_url));
  result = content::ExecJs(tab_web_contents,
                           "window.open('" + insecure_url.spec() +
                               "?popin_policy=*', '_blank', 'popin')");
  EXPECT_TRUE(
      strstr(result.message(), "Partitioned popins can only open https URLs."));

  // Insecure context and secure popin fails.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), insecure_url));
  result = content::ExecJs(tab_web_contents,
                           "window.open('" + secure_url.spec() +
                               "?popin_policy=*', '_blank', 'popin')");
  EXPECT_TRUE(strstr(result.message(),
                     "Partitioned popins must be opened from https URLs."));

  // Secure context and secure popin succeeds.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), secure_url));
  EXPECT_TRUE(content::ExecJs(tab_web_contents,
                              "window.open('" + secure_url.spec() +
                                  "?popin_policy=*', '_blank', 'popin')"));
}

// Test that a popin cannot be opened from a popin
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, PopinRecursion) {
  // Setup server.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate to a.test.
  const GURL url = embedded_https_test_server().GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open popin and verify it's visible.
  content::WebContentsAddedObserver new_tab_observer;
  EXPECT_TRUE(content::ExecJs(
      tab_web_contents,
      "window.open('" + url.spec() + "?popin_policy=*', '_blank', 'popin')"));
  content::WebContents* popin_web_contents = new_tab_observer.GetWebContents();
  BrowserWindow* popin_browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(popin_web_contents);
  EXPECT_NE(popin_browser_window, browser()->window());
  EXPECT_TRUE(popin_browser_window->IsVisible());

  // Opening a popin from a popin fails.
  const auto& result = content::ExecJs(
      popin_web_contents,
      "window.open('" + url.spec() + "?popin_policy=*', '_blank', 'popin')");
  EXPECT_TRUE(strstr(result.message(),
                     "Partitioned popins cannot open their own popin."));
}

// Test only one popin can be opened by a given context.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, PopinLimit) {
  // Setup server.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate to a.test.
  const GURL url = embedded_https_test_server().GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open popin and succeed.
  content::WebContents* popin_web_contents;
  {
    content::WebContentsAddedObserver new_tab_observer;
    EXPECT_TRUE(content::ExecJs(
        tab_web_contents,
        "window.open('" + url.spec() + "?popin_policy=*', '_blank', 'popin')"));
    popin_web_contents = new_tab_observer.GetWebContents();
    EXPECT_TRUE(popin_web_contents);
  }

  // Try to open second popin and fail.
  content::WebContentsConsoleObserver console_observer(tab_web_contents);
  console_observer.SetPattern(
      "Only one partitioned popin can be active at a time.");
  EXPECT_FALSE(
      content::EvalJs(tab_web_contents,
                      "window.open('" + url.spec() +
                          "?popin_policy=*', '_blank', 'popin') != null")
          .ExtractBool());
  ASSERT_TRUE(console_observer.Wait());

  // Close first popin and verify second can be opened.
  BrowserWindow::FindBrowserWindowWithWebContents(popin_web_contents)->Close();
  EXPECT_TRUE(
      content::EvalJs(tab_web_contents,
                      "window.open('" + url.spec() +
                          "?popin_policy=*', '_blank', 'popin') != null")
          .ExtractBool());
}

// Test that a popin cannot navigate to an HTTP page
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, PopinHttpNavigation) {
  // Setup servers.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_https_test_server().Start());
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a.test.
  const GURL secure_url =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL insecure_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), secure_url));

  // Open popin and verify it's visible.
  content::WebContentsAddedObserver new_tab_observer;
  EXPECT_TRUE(content::ExecJs(tab_web_contents,
                              "window.open('" + secure_url.spec() +
                                  "?popin_policy=*', '_blank', 'popin')"));
  content::WebContents* popin_web_contents = new_tab_observer.GetWebContents();
  BrowserWindow* popin_browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(popin_web_contents);
  EXPECT_NE(popin_browser_window, browser()->window());
  EXPECT_TRUE(popin_browser_window->IsVisible());

  // Navigating to HTTP page fails.
  content::TestNavigationObserver nav_observer(popin_web_contents);
  EXPECT_TRUE(content::ExecJs(
      popin_web_contents,
      "window.location = '" + insecure_url.spec() + "?popin_policy=*';"));
  nav_observer.Wait();
  EXPECT_EQ("chrome-error://chromewebdata/",
            content::EvalJs(popin_web_contents, "window.location.href")
                .ExtractString());
}

struct PopinPolicyTestParams {
  std::string description;
  std::string relative_url;
  bool policy_allows;
};

class BrowserNavigatorPopinPolicyTest
    : public BrowserNavigatorTest,
      public testing::WithParamInterface<
          std::tuple<PopinPolicyTestParams, PopinPolicyTestParams>> {
 public:
  PopinPolicyTestParams redirect_case() { return std::get<0>(GetParam()); }
  PopinPolicyTestParams target_case() { return std::get<1>(GetParam()); }
};

// Test that the HTTP response header `Popin-Policy` is respected.
IN_PROC_BROWSER_TEST_P(BrowserNavigatorPopinPolicyTest, PopinPolicy) {
  // Setup server.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate to a.test.
  const GURL url = embedded_https_test_server().GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open the popin and ensure the target page only loads if both the redirect
  // and the target policy permit it.
  SCOPED_TRACE(redirect_case().description + " -> " +
               target_case().description);
  content::WebContentsAddedObserver new_tab_observer;
  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(
      tab_web_contents, "window.open('" +
                            embedded_https_test_server()
                                .GetURL("a.test", redirect_case().relative_url)
                                .spec() +
                            embedded_https_test_server()
                                .GetURL("a.test", target_case().relative_url)
                                .spec() +
                            "', '_blank', 'popin')"));
  content::WebContents* popin_web_contents = new_tab_observer.GetWebContents();
  EXPECT_TRUE(popin_web_contents);
  nav_observer.Wait();
  EXPECT_EQ((redirect_case().policy_allows && target_case().policy_allows)
                ? "a.test"
                : "chromewebdata",
            content::EvalJs(popin_web_contents, "window.location.hostname")
                .ExtractString());
  BrowserWindow::FindBrowserWindowWithWebContents(popin_web_contents)->Close();
}

// Test all policy combinations.
INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    BrowserNavigatorPopinPolicyTest,
    testing::Combine(
        testing::ValuesIn(std::vector<PopinPolicyTestParams>{
            {
                "unset policy",
                "/empty.html?redirect=",
                /*policy_allows*/ false,
            },
            {
                "wildcard policy",
                "/empty.html?popin_policy=*&redirect=",
                /*policy_allows*/ true,
            },
            {
                "none policy",
                "/empty.html?popin_policy=()&redirect=",
                /*policy_allows*/ false,
            },
            {
                "origin a policy",
                "/empty.html?popin_policy=(\"https://a.test/\")&redirect=",
                /*policy_allows*/ true,
            },
            {
                "origin b policy",
                "/empty.html?popin_policy=(\"https://b.test/\")&redirect=",
                /*policy_allows*/ false,
            },
            {
                "origins policy",
                "/empty.html?popin_policy=(\"https://a.test/\" "
                "\"https://b.test/\")&redirect=",
                /*policy_allows*/ true,
            },
        }),
        testing::ValuesIn(std::vector<PopinPolicyTestParams>{
            {
                "unset policy",
                "/empty.html",
                /*policy_allows*/ false,
            },
            {
                "wildcard policy",
                "/empty.html?popin_policy=*",
                /*policy_allows*/ true,
            },
            {
                "none policy",
                "/empty.html?popin_policy=()",
                /*policy_allows*/ false,
            },
            {
                "origin a policy",
                "/empty.html?popin_policy=(\"https://a.test/\")",
                /*policy_allows*/ true,
            },
            {
                "origin b policy",
                "/empty.html?popin_policy=(\"https://b.test/\")",
                /*policy_allows*/ false,
            },
            {
                "origins policy",
                "/empty.html?popin_policy=(\"https://a.test/\" "
                "\"https://b.test/\")",
                /*policy_allows*/ true,
            },
        })));

class BrowserNavigatorPopinPolicyBypassTest
    : public BrowserNavigatorTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserNavigatorPopinPolicyBypassTest() {
    scoped_feature_list_.InitWithFeatureStates({
        {blink::features::kPartitionedPopins, true},
        {features::kPartitionedPopinsHeaderPolicyBypass,
         PartitionedPopinsHeaderPolicyBypass()},
    });
  }
  bool PartitionedPopinsHeaderPolicyBypass() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// kPartitionedPopinsHeaderPolicyBypass allows `Popin-Policy` to be bypassed.
IN_PROC_BROWSER_TEST_P(BrowserNavigatorPopinPolicyBypassTest,
                       PopinPolicyBypass) {
  // Setup server.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate to a.test.
  const GURL url = embedded_https_test_server().GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open the popin to a page without the policy and see it succeed if the
  // feature was enabled.
  content::WebContentsAddedObserver new_tab_observer;
  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(tab_web_contents, "window.open('" + url.spec() +
                                                    "', '_blank', 'popin')"));
  content::WebContents* popin_web_contents = new_tab_observer.GetWebContents();
  EXPECT_TRUE(popin_web_contents);
  nav_observer.Wait();
  EXPECT_EQ(PartitionedPopinsHeaderPolicyBypass() ? "a.test" : "chromewebdata",
            content::EvalJs(popin_web_contents, "window.location.hostname")
                .ExtractString());
  BrowserWindow::FindBrowserWindowWithWebContents(popin_web_contents)->Close();
}

// Test all policy combinations.
INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    BrowserNavigatorPopinPolicyBypassTest,
    testing::Bool());

// Test that a popin cannot navigate to an HTTP page
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, PopinHttpRedirectNavigation) {
  // Setup servers.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&PopinRequestHandler));
  ASSERT_TRUE(embedded_https_test_server().Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a.test.
  const GURL secure_url =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL insecure_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), secure_url));

  // Open popin and verify it's visible.
  content::WebContentsAddedObserver new_tab_observer;
  EXPECT_TRUE(content::ExecJs(tab_web_contents,
                              "window.open('" + secure_url.spec() +
                                  "?popin_policy=*', '_blank', 'popin')"));
  content::WebContents* popin_web_contents = new_tab_observer.GetWebContents();
  BrowserWindow* popin_browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(popin_web_contents);
  EXPECT_NE(popin_browser_window, browser()->window());
  EXPECT_TRUE(popin_browser_window->IsVisible());

  // Navigating to HTTPS page that redirects to HTTP which fails.
  content::TestNavigationObserver nav_observer(popin_web_contents);
  EXPECT_TRUE(content::ExecJs(
      popin_web_contents,
      "window.location = '" + secure_url.spec() +
          "?popin_policy=*&redirect=" + insecure_url.spec() + "';"));
  nav_observer.Wait();
  EXPECT_EQ("chrome-error://chromewebdata/",
            content::EvalJs(popin_web_contents, "window.location.href")
                .ExtractString());
}

// Verify that a popin can access cookies when opened from a cross-site context.
// This scenario was crashing before crrev.com/c/5845330
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       PopinFromCrossSiteContextAccessCookies) {
  // Setup server.
  embedded_https_test_server().SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      "content/test/data");
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate to a.test.
  const GURL url_a = embedded_https_test_server().GetURL(
      "a.test", "/partitioned_popins/iframe_allow_popins.html");
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));

  // Set a cookie for a.test.
  const GURL url_a_root = embedded_https_test_server().GetURL("a.test", "/");
  ASSERT_TRUE(
      content::SetCookie(tab_web_contents->GetBrowserContext(), url_a_root,
                         "site_a=cookie;Partitioned;SameSite=None;Secure"));

  // Navigate the iframe to b.test and set a cookie.
  const GURL url_b =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  ASSERT_TRUE(NavigateIframeToURL(tab_web_contents, "test", url_b));
  const GURL url_b_root = embedded_https_test_server().GetURL("b.test", "/");
  ASSERT_TRUE(
      content::SetCookie(tab_web_contents->GetBrowserContext(), url_b_root,
                         "site_b=cookie;Partitioned;SameSite=None;Secure"));

  // Open popin from the iframe and succeed.
  content::RenderFrameHost* iframe =
      ChildFrameAt(tab_web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  content::WebContentsAddedObserver new_tab_observer;
  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(
      iframe, "window.open('" + url_a.spec() + "', '_blank', 'popin')"));
  content::WebContents* popin_web_contents = new_tab_observer.GetWebContents();
  EXPECT_TRUE(popin_web_contents);
  nav_observer.Wait();

  // Read cookies from the popin. Only site A's cookie should be accessible.
  EXPECT_EQ(content::EvalJs(popin_web_contents, "document.cookie"),
            "site_a=cookie");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// This class extends the basic logic in display::ScreenBase to allow us to mock
// the call to `GetDisplayNearestWindow`. This provides a way to ensure that the
// opener window is on a specific display, since the display::ScreenBase
// implementation only ever returns the primary display. This is not needed on
// Ash since Ash uses DisplayManagerTestApi.
class MockScreen : public display::ScreenBase {
 public:
  MockScreen() = default;
  MockScreen(const MockScreen&) = delete;
  const MockScreen& operator=(const MockScreen&) = delete;
  ~MockScreen() override { display::Screen::SetScreenInstance(nullptr); }

  void Init() { display::Screen::SetScreenInstance(this); }

  // display::ScreenBase:
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override {
    return display_nearest_window_.value_or(
        display::ScreenBase::GetDisplayNearestWindow(window));
  }

  void set_display_nearest_window(display::Display display) {
    display_nearest_window_ = display;
  }

 private:
  std::optional<display::Display> display_nearest_window_;
};
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Windows has assumptions that the screen is a ScreenWin, which causes a crash
// when we inject the MockScreen.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BrowserNavigatorTestWithMockScreen \
  DISABLED_BrowserNavigatorTestWithMockScreen
#else
#define MAYBE_BrowserNavigatorTestWithMockScreen \
  BrowserNavigatorTestWithMockScreen
#endif
class MAYBE_BrowserNavigatorTestWithMockScreen : public BrowserNavigatorTest {
 public:
  void SetScreenInstance() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Use the default. See `SetUpOnMainThread`.
    BrowserNavigatorTest::SetScreenInstance();
#else
    mock_screen_.Init();
    mock_screen_.display_list().AddDisplay({1, gfx::Rect(0, 0, 800, 700)},
                                           display::DisplayList::Type::PRIMARY);
    mock_screen_.display_list().AddDisplay(
        {2, gfx::Rect(800, 0, 800, 700)},
        display::DisplayList::Type::NOT_PRIMARY);
    ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // This has to happen later than `SetScreenInstance` as the Ash shell does
    // not exist yet.
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay("0+0-800x700,800+0-800x700");
    ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
 protected:
  MockScreen& mock_screen() { return mock_screen_; }

 private:
  MockScreen mock_screen_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
};

IN_PROC_BROWSER_TEST_F(MAYBE_BrowserNavigatorTestWithMockScreen,
                       Disposition_PictureInPicture_OpensInSameDisplay) {
  // Create the params for the PiP request.
  auto pip_options = blink::mojom::PictureInPictureWindowOptions::New();
  pip_options->width = 500;
  pip_options->height = 400;
  WebContents::CreateParams web_contents_params(browser()->profile());
  web_contents_params.picture_in_picture_options = *pip_options;

  // Ensure we have the two displays.
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  auto display1 = display::Screen::GetScreen()->GetAllDisplays()[0];
  auto display2 = display::Screen::GetScreen()->GetAllDisplays()[1];

  {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Put the opener on display 1.
    browser()->window()->SetBounds(display1.work_area());
#else
    // Make the MockScreen report the opener as being on display 1.
    mock_screen().set_display_nearest_window(display1);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Ensure that the opener is on display 1.
    const auto opener_display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            browser()->window()->GetNativeWindow());
    ASSERT_EQ(display1.id(), opener_display.id());

    // Open the PiP window.
    NavigateParams params = MakeNavigateParams(browser());
    params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;

    // Navigate to https:// page
    net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_server.Start());
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    const GURL url = https_server.GetURL("/simple.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    params.source_contents = tab;
    params.contents_to_insert = WebContents::Create(web_contents_params);
    Navigate(&params);

    // The PiP window should also be on display 1.
    EXPECT_TRUE(
        display1.work_area().Contains(params.browser->window()->GetBounds()));
  }

  {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Put the opener on display 2.
    browser()->window()->SetBounds(display2.work_area());
#else
    // Make the MockScreen report the opener as being on display 2.
    mock_screen().set_display_nearest_window(display2);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Ensure that the opener is on display 2.
    const auto opener_display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            browser()->window()->GetNativeWindow());
    ASSERT_EQ(display2.id(), opener_display.id());

    // Open the PiP window.
    NavigateParams params = MakeNavigateParams(browser());
    params.disposition = WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;

    // Navigate to https:// page
    net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_server.Start());
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    const GURL url = https_server.GetURL("/simple.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    params.source_contents = tab;
    params.contents_to_insert = WebContents::Create(web_contents_params);
    Navigate(&params);

    // The PiP window should also be on display 2.
    EXPECT_TRUE(
        display2.work_area().Contains(params.browser->window()->GetBounds()));
  }
}

}  // namespace
