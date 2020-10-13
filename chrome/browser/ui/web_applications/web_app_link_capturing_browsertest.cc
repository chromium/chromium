// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace web_app {

class WebAppLinkCapturingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppLinkCapturingBrowserTest() = default;
  ~WebAppLinkCapturingBrowserTest() override = default;

  void SetUp() override {
    // TODO(crbug.com/1092789): Migrate the implementation to make web app link
    // capturing feature to work with AppServiceIntentHandling.
    features_.InitWithFeatures({features::kDesktopPWAsTabStrip,
                                features::kDesktopPWAsTabStripLinkCapturing},
                               {});
    WebAppNavigationBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url_;
    web_app_info->open_as_window = true;
    app_id_ = web_app::InstallWebApp(profile(), std::move(web_app_info));
    provider().registry_controller().SetExperimentalTabbedWindowMode(
        app_id_, true, /*is_user_action=*/false);
  }

  WebAppProviderBase& provider() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    DCHECK(provider);
    return *provider;
  }

  void NavigateMainBrowser(const GURL& url) {
    ClickLinkAndWait(browser()->tab_strip_model()->GetActiveWebContents(), url,
                     LinkTarget::SELF, "");
  }

  void ExpectTabs(Browser* browser, std::vector<GURL> urls) {
    TabStripModel& tab_strip = *browser->tab_strip_model();
    ASSERT_EQ(static_cast<size_t>(tab_strip.count()), urls.size());
    for (int i = 0; i < tab_strip.count(); ++i) {
      SCOPED_TRACE(base::StringPrintf("is app browser: %d, tab index: %d",
                                      bool(browser->app_controller()), i));
      EXPECT_EQ(
          browser->tab_strip_model()->GetWebContentsAt(i)->GetVisibleURL(),
          urls[i]);
    }
  }

  GURL NtpUrl() {
    return local_ntp_test_utils::GetFinalNtpUrl(browser()->profile());
  }

 protected:
  AppId app_id_;
  GURL start_url_{"https://example.org/dir/start.html"};
  GURL about_blank_{"about:blank"};
  GURL in_scope_1_{"https://example.org/dir/page1.html"};
  GURL in_scope_2_{"https://example.org/dir/page2.html"};
  GURL origin_{"https://example.org/"};
  GURL out_of_scope_{"https://other-domain.org/"};

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       InScopeNavigationsCaptured) {
  // Start browser at an out of scope page.
  NavigateMainBrowser(out_of_scope_);

  // In scope navigation should open app window.
  NavigateMainBrowser(in_scope_1_);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(app_browser, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});

  // Another in scope navigation should open a new tab in the same app window.
  NavigateMainBrowser(in_scope_2_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_});

  // Whole origin should count as in scope.
  NavigateMainBrowser(origin_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, origin_});

  // Out of scope should behave as usual.
  NavigateMainBrowser(out_of_scope_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, origin_});
}

// First about:blank captures in scope navigations.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       AboutBlankNavigationReparented) {
  ExpectTabs(browser(), {about_blank_});
  content::WebContents* reparent_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigations from a fresh about:blank page should reparent.
  // When no app window is open one should be created.
  NavigateMainBrowser(in_scope_1_);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebAppBrowser(app_browser, app_id_));
  ExpectTabs(browser(), {NtpUrl()});
  ExpectTabs(app_browser, {in_scope_1_});
  EXPECT_EQ(reparent_web_contents,
            app_browser->tab_strip_model()->GetActiveWebContents());

  // Navigations from a fresh about:blank page via JavaScript should also
  // reparent. When there is already an app window open we should reparent into
  // it.
  chrome::AddTabAt(browser(), about_blank_, /*index=*/-1, /*foreground=*/true);
  ExpectTabs(browser(), {NtpUrl(), about_blank_});
  reparent_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  {
    auto observer =
        std::make_unique<content::TestNavigationObserver>(in_scope_2_);
    observer->WatchExistingWebContents();
    ASSERT_TRUE(content::ExecuteScript(
        reparent_web_contents,
        base::StringPrintf("location = '%s';", in_scope_2_.spec().c_str())));
    observer->Wait();
  }
  ExpectTabs(browser(), {NtpUrl()});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_});
  EXPECT_EQ(reparent_web_contents,
            app_browser->tab_strip_model()->GetActiveWebContents());
}

}  // namespace web_app
