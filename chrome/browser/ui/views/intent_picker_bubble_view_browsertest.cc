// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class IntentPickerBrowserTest
    : public web_app::WebAppNavigationBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  IntentPickerBrowserTest() {
    std::vector<base::test::FeatureRef> disabled_features = {
        // TODO(crbug.com/1001189): Stop disabling Paint Holding.
        blink::features::kPaintHolding};
    scoped_feature_list_.InitWithFeatures({}, disabled_features);
  }

  template <typename Action>
  void DoAndWaitForIntentPickerIconUpdate(Action action) {
    base::RunLoop run_loop;
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    action();
    run_loop.Run();
  }

  content::WebContents* OpenNewTab(const GURL& url,
                                   const std::string& rel = "") {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    DoAndWaitForIntentPickerIconUpdate(
        [this] { NavigateToLaunchingPage(browser()); });
    DoAndWaitForIntentPickerIconUpdate([this, url, web_contents, rel] {
      TestTabActionDoesNotOpenAppWindow(
          url, base::BindOnce(&ClickLinkAndWait, web_contents, url,
                              LinkTarget::SELF, rel));
    });

    return web_contents;
  }

  // Inserts an iframe in the main frame of |web_contents|.
  bool InsertIFrame(content::WebContents* web_contents) {
    return content::ExecJs(web_contents,
                           "let iframe = document.createElement('iframe');"
                           "iframe.id = 'iframe';"
                           "document.body.appendChild(iframe);");
  }

  views::Button* GetIntentPickerIcon() {
    if (apps::features::LinkCapturingUiUpdateEnabled()) {
      return BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider()
          ->GetIntentChipButton();
    }
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kIntentPicker);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }

  size_t GetItemContainerSize(IntentPickerBubbleView* bubble) {
    return bubble->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
        ->children()
        .size();
  }

  void VerifyBubbleWithTestWebApp() {
    EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
    auto& app_info = intent_picker_bubble()->app_info_for_testing();
    ASSERT_EQ(1U, app_info.size());
    EXPECT_EQ(test_web_app_id(), app_info[0].launch_name);
    EXPECT_EQ(GetAppName(), app_info[0].display_name);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests to do with the behavior of the intent picker icon in the omnibox. Does
// not test the behavior of the intent picker bubble itself.
// Note that behavior specific to the chip version of the icon is tested
// separately in intent_chip_button_browsertest.cc.
using IntentPickerIconBrowserTest = IntentPickerBrowserTest;

// Tests that clicking a link from a tabbed browser to outside the scope of an
// installed app does not show the intent picker.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       NavigationToOutOfScopeLinkDoesNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateToLaunchingPage(browser());
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait, GetWebContents(), out_of_scope_url,
                     LinkTarget::SELF, GetParam()));

  views::Button* intent_picker_view = GetIntentPickerIcon();
  EXPECT_FALSE(intent_picker_view->GetVisible());

  EXPECT_EQ(nullptr, intent_picker_bubble());
}

// Tests that clicking a link from a tabbed browser to within the scope of an
// installed app shows the intent picker icon in Omnibox.
// TODO(crbug.com/1427908): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NavigationToInScopeLinkShowsIntentPicker \
  DISABLED_NavigationToInScopeLinkShowsIntentPicker
#else
#define MAYBE_NavigationToInScopeLinkShowsIntentPicker \
  NavigationToInScopeLinkShowsIntentPicker
#endif
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       MAYBE_NavigationToInScopeLinkShowsIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
  NavigateToLaunchingPage(browser());

  base::RunLoop run_loop;
  tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url, base::BindOnce(&ClickLinkAndWait, GetWebContents(),
                                   in_scope_url, LinkTarget::SELF, GetParam()));
  run_loop.Run();

  views::Button* intent_picker_icon = GetIntentPickerIcon();
  EXPECT_TRUE(intent_picker_icon->GetVisible());
}

// TODO(crbug.com/1395393): This test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_IconVisibilityAfterTabSwitching \
  DISABLED_IconVisibilityAfterTabSwitching
#else
#define MAYBE_IconVisibilityAfterTabSwitching IconVisibilityAfterTabSwitching
#endif
// Tests that the intent icon updates its visibility when switching between
// tabs.
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       MAYBE_IconVisibilityAfterTabSwitching) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  views::Button* intent_picker_icon = GetIntentPickerIcon();

  // OpenNewTab opens a new tab and focus on the new tab.
  OpenNewTab(in_scope_url, /*rel=*/GetParam());
  EXPECT_TRUE(intent_picker_icon->GetVisible());
  OpenNewTab(out_of_scope_url, /*rel=*/GetParam());
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(intent_picker_icon->GetVisible());
  chrome::SelectNextTab(browser());
  EXPECT_FALSE(intent_picker_icon->GetVisible());
}

// Tests that the navigation in iframe doesn't affect intent picker icon
IN_PROC_BROWSER_TEST_F(IntentPickerIconBrowserTest,
                       IframeNavigationDoesNotAffectIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  views::Button* intent_picker_icon = GetIntentPickerIcon();

  content::WebContents* initial_tab = OpenNewTab(out_of_scope_url);
  ASSERT_TRUE(InsertIFrame(initial_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", in_scope_url));
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  content::WebContents* new_tab = OpenNewTab(in_scope_url);
  ASSERT_TRUE(InsertIFrame(new_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", out_of_scope_url));
  EXPECT_TRUE(intent_picker_icon->GetVisible());
}

// TODO(crbug.com/1399441): This test is flaky. Re-enable this test.
// Tests that the intent picker icon is not visible if the navigation redirects
// to a URL that doesn't have an installed PWA.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DoesNotShowIntentPickerWhenRedirectedOutOfScope \
  DISABLED_DoesNotShowIntentPickerWhenRedirectedOutOfScope
#else
#define MAYBE_DoesNotShowIntentPickerWhenRedirectedOutOfScope \
  DoesNotShowIntentPickerWhenRedirectedOutOfScope
#endif
IN_PROC_BROWSER_TEST_P(IntentPickerIconBrowserTest,
                       MAYBE_DoesNotShowIntentPickerWhenRedirectedOutOfScope) {
  InstallTestWebApp(GetOtherAppUrlHost(), /*app_scope=*/"/");

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  const GURL in_scope_url = https_server().GetURL(GetOtherAppUrlHost(), "/");
  const GURL redirect_url = https_server().GetURL(
      GetOtherAppUrlHost(), CreateServerRedirect(out_of_scope_url));

  views::Button* intent_picker_icon = GetIntentPickerIcon();

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_icon->GetVisible());

  DoAndWaitForIntentPickerIconUpdate([this, redirect_url, out_of_scope_url] {
    ClickLinkAndWaitForURL(GetWebContents(), redirect_url, out_of_scope_url,
                           LinkTarget::SELF, GetParam());
  });
  EXPECT_FALSE(intent_picker_icon->GetVisible());
}

// Test that navigating to service pages (chrome://) will hide the intent picker
// icon.
IN_PROC_BROWSER_TEST_F(IntentPickerIconBrowserTest,
                       DoNotShowIconAndBubbleOnServicePages) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  GURL chrome_pages_url("chrome://version");
  std::string app_name = "test_name";

  views::Button* intent_picker_view = GetIntentPickerIcon();

  OpenNewTab(in_scope_url);
  ASSERT_TRUE(intent_picker_view);
  EXPECT_TRUE(intent_picker_view->GetVisible());

  // Now switch to chrome://version.
  DoAndWaitForIntentPickerIconUpdate([this, &chrome_pages_url]() {
    NavigateParams params(browser(), chrome_pages_url,
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    // Navigates and waits for loading to finish.
    ui_test_utils::NavigateToURL(&params);
  });

  // Make sure that the intent picker icon is no longer visible.
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Test that error pages do not show the intent picker icon.
IN_PROC_BROWSER_TEST_F(IntentPickerIconBrowserTest, DoNotShowIconOnErrorPages) {
  InstallTestWebApp();
  InstallTestWebApp("www.google.com", "/");

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  views::Button* intent_picker_view = GetIntentPickerIcon();
  ASSERT_TRUE(intent_picker_view);

  // Go to the test app and wait for the intent picker icon to load.
  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_view->GetVisible());

  // Now switch to www.google.com, which gives a network error in the test
  // environment.
  DoAndWaitForIntentPickerIconUpdate([this]() {
    NavigateParams params(browser(), GURL("https://www.google.com"),
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    // Navigates and waits for loading to finish.
    ui_test_utils::NavigateToURL(&params);
  });

  // Make sure that the intent picker icon is not shown on the error page, even
  // though there's a PWA available for www.google.com.
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Test that loading a page with pushState() call that changes URL updates the
// intent picker view.
IN_PROC_BROWSER_TEST_F(IntentPickerIconBrowserTest, PushStateURLChangeTest) {
  // Note: The test page is served from embedded_test_server() as https_server()
  // always returns empty responses.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server().Start());

  const GURL test_url =
      embedded_test_server()->GetURL("/intent_picker/push_state_test.html");
  web_app::test::InstallDummyWebApp(profile(), "Test app", test_url);

  views::Button* intent_picker_view = GetIntentPickerIcon();

  OpenNewTab(test_url);
  EXPECT_TRUE(intent_picker_view->GetVisible());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DoAndWaitForIntentPickerIconUpdate([web_contents] {
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.getElementById('push_to_new_url_button').click();"));
  });

  EXPECT_FALSE(intent_picker_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(IntentPickerIconBrowserTest, OpenBubbleOnClick) {
  InstallTestWebApp();
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       IntentPickerBubbleView::kViewClassName);

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());

  views::Button* intent_picker_icon = GetIntentPickerIcon();

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_icon->GetVisible());

  views::test::ButtonTestApi test_api(intent_picker_icon);
  test_api.NotifyClick(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(intent_picker_bubble());
  VerifyBubbleWithTestWebApp();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerIconBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

class IntentPickerIconPrerenderingBrowserTest
    : public IntentPickerIconBrowserTest {
 public:
  IntentPickerIconPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &IntentPickerIconPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~IntentPickerIconPrerenderingBrowserTest() override = default;
  IntentPickerIconPrerenderingBrowserTest(
      const IntentPickerIconPrerenderingBrowserTest&) = delete;

  IntentPickerIconPrerenderingBrowserTest& operator=(
      const IntentPickerIconPrerenderingBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    IntentPickerIconBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    IntentPickerIconBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_P(IntentPickerIconPrerenderingBrowserTest,
                       PrerenderingShouldNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL initial_url =
      https_server().GetURL(GetAppUrlHost(), "/empty.html");
  OpenNewTab(initial_url);

  views::Button* intent_picker_icon = GetIntentPickerIcon();
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  // Load a prerender page and prerendering should not try to show the
  // intent picker.
  const GURL prerender_url = https_server().GetURL(
      GetAppUrlHost(), std::string(GetAppScopePath()) + "index1.html");
  int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  // Activate the prerender page.
  DoAndWaitForIntentPickerIconUpdate([this, prerender_url] {
    prerender_test_helper().NavigatePrimaryPage(prerender_url);
  });
  EXPECT_TRUE(host_observer.was_activated());

  // After activation, IntentPickerTabHelper should show the
  // intent picker.
  EXPECT_TRUE(intent_picker_icon->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerIconPrerenderingBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

class IntentPickerIconFencedFrameBrowserTest
    : public IntentPickerIconBrowserTest {
 public:
  IntentPickerIconFencedFrameBrowserTest() = default;
  ~IntentPickerIconFencedFrameBrowserTest() override = default;
  IntentPickerIconFencedFrameBrowserTest(
      const IntentPickerIconFencedFrameBrowserTest&) = delete;

  IntentPickerIconFencedFrameBrowserTest& operator=(
      const IntentPickerIconFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_P(IntentPickerIconFencedFrameBrowserTest,
                       ShouldShowIntentPickerInFencedFrame) {
  InstallTestWebApp();

  views::Button* intent_picker_icon = GetIntentPickerIcon();

  const GURL initial_url =
      https_server().GetURL(GetAppUrlHost(), "/empty.html");
  OpenNewTab(initial_url);
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  const GURL fenced_frame_url = https_server().GetURL(
      GetAppUrlHost(), std::string(GetAppScopePath()) + "index1.html");
  // Create a fenced frame.
  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url));

  EXPECT_FALSE(intent_picker_icon->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerIconFencedFrameBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));
