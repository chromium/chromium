// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class CookieControlsBubbleViewTest : public DialogBrowserTest {
 public:
  CookieControlsBubbleViewTest() = default;

  CookieControlsBubbleViewTest(const CookieControlsBubbleViewTest&) = delete;
  CookieControlsBubbleViewTest& operator=(const CookieControlsBubbleViewTest&) =
      delete;

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
    content::CookieChangeObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    NavigateToUrlWithThirdPartyCookies();
    if (name == "NotWorkingClicked") {
      observer.Wait();
    }
    ASSERT_TRUE(cookie_controls_icon()->GetVisible());
    cookie_controls_icon_->ExecuteForTesting();

    auto* bubble = static_cast<CookieControlsBubbleView*>(
        cookie_controls_icon_->GetBubble());

    if (name == "NotWorkingClicked") {
      views::View* link = bubble->parent()->GetViewByID(
          CookieControlsBubbleView::VIEW_ID_COOKIE_CONTROLS_NOT_WORKING_LINK);
      ASSERT_TRUE(link);
      link->OnKeyPressed(
          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
    }
  }

  void AcceptBubble() {
    auto* bubble = static_cast<CookieControlsBubbleView*>(
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
  raw_ptr<PageActionIconView, DanglingUntriaged> cookie_controls_icon_;
};

// Test that cookie icon is not shown when cookies are not blocked.
IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewTest, NoCookiesBlocked) {
  NavigateToUrlWithThirdPartyCookies();
  EXPECT_FALSE(cookie_controls_icon()->GetVisible());
}

// Test opening cookie controls bubble and clicking on "not working" link.
// Check that accepting the bubble unblocks 3p cookies for this origin.
// TODO(https://crbug.com/1309497): Flaky on win and mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_NotWorkingClicked DISABLED_NotWorkingClicked
#else
#define MAYBE_NotWorkingClicked NotWorkingClicked
#endif
IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewTest, MAYBE_NotWorkingClicked) {
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

// Test opening cookie controls bubble while 3p cookies are allowed for this
// page. Check that accepting the bubble blocks cookies again.
IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewTest, BlockingDisabled) {
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

// ==================== Pixel tests ====================

// Test opening cookie controls bubble.
// TODO(https://crbug.com/1309905):  Flakily fails on multiple platforms
IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewTest, DISABLED_InvokeUi_CookiesBlocked) {
  SetThirdPartyCookieBlocking(true);
  ShowAndVerifyUi();
}

// Test opening cookie controls bubble and clicking on "not working" link.
// TODO(https://crbug.com/1332525): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewTest,
                       DISABLED_InvokeUi_NotWorkingClicked) {
  // Block 3p cookies.
  SetThirdPartyCookieBlocking(true);

  // Open bubble.
  ShowAndVerifyUi();
}

// Test opening cookie controls bubble while 3p cookies are allowed for this
// page.
IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewTest,
                       InvokeUi_BlockingDisabled) {
  // Block 3p cookies in general but allow them for this site.
  SetThirdPartyCookieBlocking(true);
  GURL origin = embedded_test_server()->GetURL("a.com", "/");
  cookie_settings()->SetThirdPartyCookieSetting(
      origin, ContentSetting::CONTENT_SETTING_ALLOW);

  // Show bubble.
  ShowAndVerifyUi();
}
