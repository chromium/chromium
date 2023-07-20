// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

class CookieControlsBubbleViewDialogBrowserTest : public DialogBrowserTest {
 public:
  CookieControlsBubbleViewDialogBrowserTest() {
    scoped_feature_list.InitAndEnableFeature(
        content_settings::features::kUserBypassUI);
  }

  void TearDownOnMainThread() override {
    cookie_controls_coordinator_ = nullptr;
    cookie_controls_icon_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  CookieControlsBubbleViewDialogBrowserTest(
      const CookieControlsBubbleViewDialogBrowserTest&) = delete;
  CookieControlsBubbleViewDialogBrowserTest& operator=(
      const CookieControlsBubbleViewDialogBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    cookie_controls_icon_ = static_cast<CookieControlsIconView*>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kCookieControls));
    ASSERT_TRUE(cookie_controls_icon_);
    cookie_controls_coordinator_ =
        cookie_controls_icon_->GetCoordinatorForTesting();
    cookie_controls_coordinator_->SetDisplayNameForTesting(u"example.com");
  }

  void ShowUi(const std::string& name) override {
    if (name == "CookiesBlocked") {
      SetThirdPartyCookieBlocking(true);
      GURL origin = embedded_test_server()->GetURL("a.com", "/");
      EXPECT_FALSE(
          cookie_settings()->IsThirdPartyAccessAllowed(origin, nullptr));

      NavigateToUrlWithThirdPartyCookies();
    }

    ASSERT_TRUE(cookie_controls_icon()->GetVisible());
    cookie_controls_icon()->ExecuteForTesting();
  }

  void NavigateToUrlWithThirdPartyCookies() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::CookieChangeObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/iframe.html")));
    EXPECT_TRUE(NavigateIframeToURL(
        web_contents, "test",
        embedded_test_server()->GetURL("b.com", "/setcookie.html")));
    observer.Wait();
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
  base::test::ScopedFeatureList scoped_feature_list;
  raw_ptr<CookieControlsIconView> cookie_controls_icon_;
  raw_ptr<CookieControlsBubbleCoordinator> cookie_controls_coordinator_;
};

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewDialogBrowserTest,
                       InvokeUi_CookiesBlocked) {
  ShowAndVerifyUi();
}
