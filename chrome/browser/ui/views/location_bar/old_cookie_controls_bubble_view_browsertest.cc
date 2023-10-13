// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/old_cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class OldCookieControlsBubbleViewTest : public DialogBrowserTest {
 public:
  OldCookieControlsBubbleViewTest() {
    feature_list_.InitAndDisableFeature(
        content_settings::features::kUserBypassUI);
  }

  OldCookieControlsBubbleViewTest(const OldCookieControlsBubbleViewTest&) =
      delete;
  OldCookieControlsBubbleViewTest& operator=(
      const OldCookieControlsBubbleViewTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    cookie_controls_icon_ =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kCookieControls);
    ASSERT_TRUE(cookie_controls_icon_);
  }

  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    if (name == "StatefulBounce") {
      NavigateToUrlWithStatefulBounce();
    } else {
      content::CookieChangeObserver observer(web_contents);
      NavigateToUrlWithThirdPartyCookies();
      if (name == "NotWorkingClicked" || name == "CookiesBlocked") {
        observer.Wait();
      }
    }

    ASSERT_TRUE(cookie_controls_icon()->GetVisible());
    cookie_controls_icon_->ExecuteForTesting();

    auto* bubble = static_cast<OldCookieControlsBubbleView*>(
        cookie_controls_icon_->GetBubble());

    if (name == "NotWorkingClicked") {
      views::View* link = bubble->parent()->GetViewByID(
          OldCookieControlsBubbleView::
              VIEW_ID_COOKIE_CONTROLS_NOT_WORKING_LINK);
      ASSERT_TRUE(link);
      link->OnKeyPressed(
          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
    }
  }

  void AcceptBubble() {
    auto* bubble = static_cast<OldCookieControlsBubbleView*>(
        cookie_controls_icon_->GetBubble());
    ASSERT_TRUE(bubble);
    bubble->Accept();
  }

  void NavigateToUrlWithThirdPartyCookies() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/iframe.html")));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(
        web_contents, "test",
        embedded_test_server()->GetURL("b.com", "/setcookie.html")));
  }

  void NavigateToUrlWithStatefulBounce() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Initial navigation to a.com.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));
    // Bounce to tracking site b.com.
    ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
        web_contents, embedded_test_server()->GetURL("b.com", "/title1.html")));

    // TODO(crbug.com/1447854): This extra navigation is necessary for the test
    // to end the redirect on a.com. Investigate why.
    ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
        web_contents, embedded_test_server()->GetURL("a.com", "/title1.html")));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));
    ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
        web_contents, embedded_test_server()->GetURL("b.com", "/title1.html")));

    // Include a stateful access from b.com.
    ASSERT_TRUE(content::ExecJs(
        web_contents->GetPrimaryMainFrame(),
        base::StringPrintf(kStorageAccessScript, "LocalStorage"),
        content::EXECUTE_SCRIPT_NO_USER_GESTURE,
        /*world_id=*/1));

    // Bounce back to a.com.
    ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
        web_contents, embedded_test_server()->GetURL("a.com", "/title1.html")));
    // Terminate the redirect chain.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  }

  void SetThirdPartyCookieBlocking(bool enabled) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            enabled ? content_settings::CookieControlsMode::kBlockThirdParty
                    : content_settings::CookieControlsMode::kOff));
  }

  scoped_refptr<content_settings::CookieSettings> cookie_settings() {
    return CookieSettingsFactory::GetForProfile(browser()->profile());
  }

  PageActionIconView* cookie_controls_icon() { return cookie_controls_icon_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<PageActionIconView, AcrossTasksDanglingUntriaged>
      cookie_controls_icon_;
};

// Test that cookie icon is not shown when cookies are not blocked.
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest, NoCookiesBlocked) {
  NavigateToUrlWithThirdPartyCookies();
  EXPECT_FALSE(cookie_controls_icon()->GetVisible());
}

// Test opening cookie controls bubble and clicking on "not working" link.
// Check that accepting the bubble unblocks 3p cookies for this origin.
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest, NotWorkingClicked) {
  // Block 3p cookies.
  SetThirdPartyCookieBlocking(true);
  GURL origin = embedded_test_server()->GetURL("a.com", "/");
  EXPECT_FALSE(cookie_settings()->IsThirdPartyAccessAllowed(origin, nullptr));

  // Open bubble.
  ShowUi("NotWorkingClicked");

  // Allow cookies for this site by accepting bubble.
  AcceptBubble();
  EXPECT_TRUE(cookie_settings()->IsThirdPartyAccessAllowed(origin, nullptr));
}

// Test that opening cookie controls bubble sets
// `prefs::kInContextCookieControlsOpened`.
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest,
                       InContextCookieControlsOpenedRecorded) {
  // Block 3p cookies.
  SetThirdPartyCookieBlocking(true);
  GURL origin = embedded_test_server()->GetURL("a.com", "/");
  EXPECT_FALSE(cookie_settings()->IsThirdPartyAccessAllowed(origin, nullptr));

  // The pref is false before opening the bubble.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kInContextCookieControlsOpened));

  // Open bubble.
  ShowUi("CookiesBlocked");

  // The pref is true after opening the bubble.
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kInContextCookieControlsOpened));
}

// Test opening cookie controls bubble while 3p cookies are allowed for this
// page. Check that accepting the bubble blocks cookies again.
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest, BlockingDisabled) {
  // Block 3p cookies in general but allow them for this site.
  SetThirdPartyCookieBlocking(true);
  GURL origin = embedded_test_server()->GetURL("a.com", "/");
  cookie_settings()->SetThirdPartyCookieSetting(
      origin, ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(cookie_settings()->IsThirdPartyAccessAllowed(origin, nullptr));

  // Show bubble.
  ShowUi("");

  // Block cookies again by accepting the bubble.
  AcceptBubble();
  EXPECT_FALSE(cookie_settings()->IsThirdPartyAccessAllowed(origin, nullptr));
}

IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest, NonAllowedCookieSite) {
  // Regression test for crbug.com/1459383. Activating a tab where cookies are
  // blocked, such as an internal chrome:// url, while the UI is shown, should
  // not crash.
  SetThirdPartyCookieBlocking(true);

  // Open chrome://about in the background.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://about"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Navigate current tab and open bubble on the page with 3PC blocked.
  ShowUi("NotWorkingClicked");

  // While bubble is open, activate the chrome://about tab.
  browser()->tab_strip_model()->ActivateTabAt(1);

  // The crash would have already occurred, but navigate to another chrome://
  // url to be sure.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));
}

// ==================== Pixel tests ====================

// Test opening cookie controls bubble with blocked cookies present.
// TODO(crbug.com/1432008): Failing on Linux ChromeOS debug build.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InvokeUi_CookiesBlocked DISABLED_InvokeUi_CookiesBlocked
#else
#define MAYBE_InvokeUi_CookiesBlocked InvokeUi_CookiesBlocked
#endif
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest,
                       MAYBE_InvokeUi_CookiesBlocked) {
  SetThirdPartyCookieBlocking(true);
  ShowAndVerifyUi();
}

// Test opening cookie controls bubble after a stateful redirect.
// TODO(crbug.com/1447854): Flaky on Chrome OS and Linux builds.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_InvokeUi_StatefulBounce DISABLED_InvokeUi_StatefulBounce
#else
#define MAYBE_InvokeUi_StatefulBounce InvokeUi_StatefulBounce
#endif
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest,
                       MAYBE_InvokeUi_StatefulBounce) {
  SetThirdPartyCookieBlocking(true);
  ShowAndVerifyUi();
}

// Test opening cookie controls bubble and clicking on "not working" link.
// TODO(crbug.com/1332525): Failing on Linux ChromeOS debug build.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InvokeUi_NotWorkingClicked DISABLED_InvokeUi_NotWorkingClicked
#else
#define MAYBE_InvokeUi_NotWorkingClicked InvokeUi_NotWorkingClicked
#endif
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest,
                       MAYBE_InvokeUi_NotWorkingClicked) {
  // Block 3p cookies.
  SetThirdPartyCookieBlocking(true);

  // Open bubble.
  ShowAndVerifyUi();
}

// Test opening cookie controls bubble while 3p cookies are allowed for this
// page.
// TODO(crbug.com/1332525): Failing on Linux ChromeOS debug build.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InvokeUi_BlockingDisabled DISABLED_InvokeUi_BlockingDisabled
#else
#define MAYBE_InvokeUi_BlockingDisabled InvokeUi_BlockingDisabled
#endif
IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest,
                       MAYBE_InvokeUi_BlockingDisabled) {
  // Block 3p cookies in general but allow them for this site.
  SetThirdPartyCookieBlocking(true);
  GURL origin = embedded_test_server()->GetURL("a.com", "/");
  cookie_settings()->SetThirdPartyCookieSetting(
      origin, ContentSetting::CONTENT_SETTING_ALLOW);

  // Show bubble.
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(OldCookieControlsBubbleViewTest,
                       IconViewAccessibleName) {
  EXPECT_FALSE(cookie_controls_icon()->GetVisible());
  EXPECT_EQ(cookie_controls_icon()->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TOOLTIP));
  EXPECT_EQ(cookie_controls_icon()->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TOOLTIP));
}
