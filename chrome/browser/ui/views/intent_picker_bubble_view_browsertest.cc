// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class IntentPickerBubbleViewBrowserTest
    : public web_app::WebAppNavigationBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  void SetUp() override {
    // TODO(schenney): Stop disabling Paint Holding. crbug.com/1001189
    scoped_feature_list_.InitAndDisableFeature(blink::features::kPaintHolding);
    web_app::WebAppNavigationBrowserTest::SetUp();
  }

  void OpenNewTab(const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    NavigateToLaunchingPage(browser());
    TestTabActionDoesNotOpenAppWindow(
        url, base::BindOnce(&ClickLinkAndWait, web_contents, url,
                            LinkTarget::SELF, GetParam()));
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

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }

  void VerifyBubbleWithTestWebApp() {
    EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
    auto& app_info = intent_picker_bubble()->app_info_for_testing();
    ASSERT_EQ(1U, app_info.size());
    EXPECT_EQ(test_web_app_id(), app_info[0].launch_name);
    EXPECT_EQ(GetAppName(), app_info[0].display_name);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that clicking a link from a tabbed browser to within the scope of an
// installed app shows the intent picker icon in Omnibox. The intent picker
// bubble will only show up for android apps which is too hard to test.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToInScopeLinkShowsIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateToLaunchingPage(browser());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url, base::BindOnce(&ClickLinkAndWait, web_contents,
                                   in_scope_url, LinkTarget::SELF, GetParam()));

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();
  EXPECT_TRUE(intent_picker_view->GetVisible());

  if (!base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence)) {
    EXPECT_FALSE(intent_picker_bubble());
    GetIntentPickerIcon()->ExecuteForTesting();
  }

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());

  VerifyBubbleWithTestWebApp();

  intent_picker_bubble()->AcceptDialog();

  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}

// Tests that clicking a link from a tabbed browser to outside the scope of an
// installed app does not show the intent picker.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToOutofScopeLinkDoesNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateToLaunchingPage(browser());
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     out_of_scope_url, LinkTarget::SELF, GetParam()));

  EXPECT_EQ(nullptr, intent_picker_bubble());
}

// Tests that clicking a link from an app browser to either within or outside
// the scope of an installed app does not show the intent picker, even when an
// outside of scope link is opened within the context of the PWA.
// Flaky on Linux: https://crbug.com/1186613
#if defined(OS_LINUX)
#define MAYBE_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker \
  DISABLED_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker
#else
#define MAYBE_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker \
  NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker
#endif
IN_PROC_BROWSER_TEST_P(
    IntentPickerBubbleViewBrowserTest,
    MAYBE_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker) {
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

// Tests that the intent icon updates its visibiliy when switching between
// tabs.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  // OpenNewTab opens a new tab and focus on the new tab.
  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_view->GetVisible());
  OpenNewTab(out_of_scope_url);
  EXPECT_FALSE(intent_picker_view->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(intent_picker_view->GetVisible());

  chrome::SelectNextTab(browser());
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Tests that the navigation in iframe doesn't affect intent picker icon
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       IframeNavigationDoesNotAffectIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  OpenNewTab(out_of_scope_url);
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(InsertIFrame(initial_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", in_scope_url));
  EXPECT_FALSE(intent_picker_view->GetVisible());

  OpenNewTab(in_scope_url);
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(InsertIFrame(new_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", out_of_scope_url));
  EXPECT_TRUE(intent_picker_view->GetVisible());
}

// Tests that the intent picker icon is not visible if the navigatation
// redirects to a URL that doesn't have an installed PWA.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       DoesNotShowIntentPickerWhenRedirectedOutOfScope) {
  InstallTestWebApp(GetOtherAppUrlHost(), /*app_scope=*/"/");

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  const GURL in_scope_url = https_server().GetURL(GetOtherAppUrlHost(), "/");
  const GURL redirect_url = https_server().GetURL(
      GetOtherAppUrlHost(), CreateServerRedirect(out_of_scope_url));

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_view->GetVisible());

  ClickLinkAndWaitForURL(browser()->tab_strip_model()->GetActiveWebContents(),
                         redirect_url, out_of_scope_url, LinkTarget::SELF,
                         GetParam());
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTest, DoubleClickOpensApp) {
  auto app_id = InstallTestWebApp(GetAppUrlHost(), GetAppScopePath());

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ui_test_utils::NavigateToURL(browser(), in_scope_url);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  GetIntentPickerIcon()->ExecuteForTesting();
  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());

  intent_picker_bubble()->PressButtonForTesting(
      /* index= */ 0,
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0));
  intent_picker_bubble()->PressButtonForTesting(
      /* index= */ 0,
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_IS_DOUBLE_CLICK, 0));

  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerBubbleViewBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));
