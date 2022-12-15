// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
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
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class IntentPickerBubbleViewBrowserTest
    : public web_app::WebAppNavigationBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  IntentPickerBubbleViewBrowserTest() {
    std::vector<base::test::FeatureRef> disabled_features = {
        // TODO(schenney): Stop disabling Paint Holding. crbug.com/1001189
        blink::features::kPaintHolding,
        // TODO(crbug.com/1357905): Run relevant tests against the updated UI.
        apps::features::kLinkCapturingUiUpdate};
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

  content::WebContents* OpenNewTab(const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    DoAndWaitForIntentPickerIconUpdate(
        [this] { NavigateToLaunchingPage(browser()); });
    DoAndWaitForIntentPickerIconUpdate([this, url, web_contents] {
      TestTabActionDoesNotOpenAppWindow(
          url, base::BindOnce(&ClickLinkAndWait, web_contents, url,
                              LinkTarget::SELF, GetParam()));
    });

    return web_contents;
  }

  // Inserts an iframe in the main frame of |web_contents|.
  bool InsertIFrame(content::WebContents* web_contents) {
    return content::ExecuteScript(
        web_contents,
        "let iframe = document.createElement('iframe');"
        "iframe.id = 'iframe';"
        "document.body.appendChild(iframe);");
  }

  PageActionIconView* GetIntentPickerIcon() {
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

// Tests that clicking a link from a tabbed browser to outside the scope of an
// installed app does not show the intent picker.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToOutOfScopeLinkDoesNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateToLaunchingPage(browser());
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait, GetWebContents(), out_of_scope_url,
                     LinkTarget::SELF, GetParam()));

  EXPECT_EQ(nullptr, intent_picker_bubble());
}

// TODO(crbug.com/1252812): Enable the following two tests on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that clicking a link from a tabbed browser to within the scope of an
// installed app shows the intent picker icon in Omnibox.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToInScopeLinkShowsIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
  NavigateToLaunchingPage(browser());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  base::RunLoop run_loop;
  tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url, base::BindOnce(&ClickLinkAndWait, GetWebContents(),
                                   in_scope_url, LinkTarget::SELF, GetParam()));
  run_loop.Run();

  PageActionIconView* intent_picker_icon = GetIntentPickerIcon();
  EXPECT_TRUE(intent_picker_icon->GetVisible());

#if !BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, the picker bubble will appear automatically.
  EXPECT_FALSE(intent_picker_bubble());
  intent_picker_icon->ExecuteForTesting();
#endif

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());

  VerifyBubbleWithTestWebApp();

  intent_picker_bubble()->AcceptDialog();

  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests that clicking a link from an app browser to either within or outside
// the scope of an installed app does not show the intent picker, even when an
// outside of scope link is opened within the context of the PWA.
IN_PROC_BROWSER_TEST_P(
    IntentPickerBubbleViewBrowserTest,
    NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker) {
  InstallTestWebApp();

  // No intent picker should be seen when first opening the web app.
  Browser* app_browser = OpenTestWebApp();
  EXPECT_EQ(nullptr, intent_picker_bubble());

  {
    const GURL in_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
    TestActionDoesNotOpenAppWindow(
        app_browser, in_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       in_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, intent_picker_bubble());
  }

  {
    const GURL out_of_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
    TestActionDoesNotOpenAppWindow(
        app_browser, out_of_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       out_of_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, intent_picker_bubble());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(crbug.com/1395393): This test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_IconVisibilityAfterTabSwitching \
  DISABLED_IconVisibilityAfterTabSwitching
#else
#define MAYBE_IconVisibilityAfterTabSwitching IconVisibilityAfterTabSwitching
#endif
// Tests that the intent icon updates its visibility when switching between
// tabs.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       MAYBE_IconVisibilityAfterTabSwitching) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  PageActionIconView* intent_picker_icon = GetIntentPickerIcon();

  // OpenNewTab opens a new tab and focus on the new tab.
  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_icon->GetVisible());
  OpenNewTab(out_of_scope_url);
  EXPECT_FALSE(intent_picker_icon->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(intent_picker_icon->GetVisible());
  chrome::SelectNextTab(browser());
  EXPECT_FALSE(intent_picker_icon->GetVisible());
}

// Tests that the navigation in iframe doesn't affect intent picker icon
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       IframeNavigationDoesNotAffectIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  PageActionIconView* intent_picker_icon = GetIntentPickerIcon();

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
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       MAYBE_DoesNotShowIntentPickerWhenRedirectedOutOfScope) {
  InstallTestWebApp(GetOtherAppUrlHost(), /*app_scope=*/"/");

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  const GURL in_scope_url = https_server().GetURL(GetOtherAppUrlHost(), "/");
  const GURL redirect_url = https_server().GetURL(
      GetOtherAppUrlHost(), CreateServerRedirect(out_of_scope_url));

  PageActionIconView* intent_picker_icon = GetIntentPickerIcon();

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_icon->GetVisible());

  DoAndWaitForIntentPickerIconUpdate([this, redirect_url, out_of_scope_url] {
    ClickLinkAndWaitForURL(GetWebContents(), redirect_url, out_of_scope_url,
                           LinkTarget::SELF, GetParam());
  });
  EXPECT_FALSE(intent_picker_icon->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerBubbleViewBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

class IntentPickerBubbleViewPrerenderingBrowserTest
    : public IntentPickerBubbleViewBrowserTest {
 public:
  IntentPickerBubbleViewPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &IntentPickerBubbleViewPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~IntentPickerBubbleViewPrerenderingBrowserTest() override = default;
  IntentPickerBubbleViewPrerenderingBrowserTest(
      const IntentPickerBubbleViewPrerenderingBrowserTest&) = delete;

  IntentPickerBubbleViewPrerenderingBrowserTest& operator=(
      const IntentPickerBubbleViewPrerenderingBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    IntentPickerBubbleViewBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    IntentPickerBubbleViewBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewPrerenderingBrowserTest,
                       PrerenderingShouldNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL initial_url =
      https_server().GetURL(GetAppUrlHost(), "/empty.html");
  OpenNewTab(initial_url);

  PageActionIconView* intent_picker_icon = GetIntentPickerIcon();
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
    IntentPickerBubbleViewPrerenderingBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

class IntentPickerBubbleViewFencedFrameBrowserTest
    : public IntentPickerBubbleViewBrowserTest {
 public:
  IntentPickerBubbleViewFencedFrameBrowserTest() = default;
  ~IntentPickerBubbleViewFencedFrameBrowserTest() override = default;
  IntentPickerBubbleViewFencedFrameBrowserTest(
      const IntentPickerBubbleViewFencedFrameBrowserTest&) = delete;

  IntentPickerBubbleViewFencedFrameBrowserTest& operator=(
      const IntentPickerBubbleViewFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewFencedFrameBrowserTest,
                       ShouldShowIntentPickerInFencedFrame) {
  InstallTestWebApp();

  PageActionIconView* intent_picker_icon = GetIntentPickerIcon();

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
    IntentPickerBubbleViewFencedFrameBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));
