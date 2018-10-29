// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/bookmark_app_navigation_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "url/gurl.h"

class IntentPickerBubbleViewBrowserTest
    : public extensions::test::BookmarkAppNavigationBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  void SetUp() override {
    extensions::test::BookmarkAppNavigationBrowserTest::SetUp();

    // Link capturing disables showing the intent picker.
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAWindowing},
        {features::kDesktopPWAsLinkCapturing});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that clicking a link from a tabbed browser to within the scope of an
// installed app shows the intent picker with the installed app details.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToInScopeLinkShowsIntentPicker) {
  InstallTestBookmarkApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateToLaunchingPage(browser());
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url, base::BindOnce(&ClickLinkAndWait, web_contents,
                                   in_scope_url, LinkTarget::SELF, GetParam()));

  IntentPickerBubbleView* intent_picker =
      IntentPickerBubbleView::intent_picker_bubble();
  EXPECT_TRUE(intent_picker);
  EXPECT_EQ(web_contents, intent_picker->web_contents());
  EXPECT_EQ(1u, intent_picker->GetAppInfoForTesting().size());
  EXPECT_EQ(GetAppName(),
            intent_picker->GetAppInfoForTesting()[0].display_name);
}

// Tests that clicking a link from a tabbed browser to outside the scope of an
// installed app does not show the intent picker.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToOutofScopeLinkDoesNotShowIntentPicker) {
  InstallTestBookmarkApp();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateToLaunchingPage(browser());
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     out_of_scope_url, LinkTarget::SELF, GetParam()));

  EXPECT_EQ(nullptr, IntentPickerBubbleView::intent_picker_bubble());
}

// Tests that clicking a link from an app browser to either within or outside
// the scope of an installed app does not show the intent picker, even when the
// outside of scope link opens a new tabbed browser.
IN_PROC_BROWSER_TEST_P(
    IntentPickerBubbleViewBrowserTest,
    NavigationInAppWindowToInScopeLinkDoesNotShowIntentPickerWhenDesktopPWAsStayInWindowDisabled) {
  // Disable the desktop-pwas-stay-in-window flag, so we can test that links
  // opening in the browser don't trigger the intent picker.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kDesktopPWAsStayInWindow);

  InstallTestBookmarkApp();

  // No intent picker should be seen when first opening the bookmark app.
  Browser* app_browser = OpenTestBookmarkApp();
  EXPECT_EQ(nullptr, IntentPickerBubbleView::intent_picker_bubble());

  {
    const GURL in_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
    TestActionDoesNotOpenAppWindow(
        app_browser, in_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       in_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, IntentPickerBubbleView::intent_picker_bubble());
  }

  {
    const GURL out_of_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
    TestAppActionOpensForegroundTab(
        app_browser, out_of_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       out_of_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, IntentPickerBubbleView::intent_picker_bubble());
  }
}

// Tests that clicking a link from an app browser to either within or outside
// the scope of an installed app does not show the intent picker, even when an
// outside of scope link is opened within the context of the PWA.
IN_PROC_BROWSER_TEST_P(
    IntentPickerBubbleViewBrowserTest,
    NavigationInAppWindowToInScopeLinkDoesNotShowIntentPickerWhenDesktopPWAsStayInWindowEnabled) {
  // Enable desktop-pwas-stay-in-window, so we can test that links opening in
  // the PWA which are out of scope don't trigger the intent picker.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDesktopPWAsStayInWindow);

  InstallTestBookmarkApp();

  // No intent picker should be seen when first opening the bookmark app.
  Browser* app_browser = OpenTestBookmarkApp();
  EXPECT_EQ(nullptr, IntentPickerBubbleView::intent_picker_bubble());

  {
    const GURL in_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
    TestActionDoesNotOpenAppWindow(
        app_browser, in_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       in_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, IntentPickerBubbleView::intent_picker_bubble());
  }

  {
    const GURL out_of_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
    TestActionDoesNotOpenAppWindow(
        app_browser, out_of_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       out_of_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, IntentPickerBubbleView::intent_picker_bubble());
  }
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    IntentPickerBubbleViewBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));
