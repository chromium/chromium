// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/label.h"

class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit TestWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~TestWebContentsObserver() override = default;

  void DidStartLoading() override {
    if (start_loading_callback_) {
      std::move(start_loading_callback_).Run();
    }
  }

  void SetStartLoadingCallback(base::OnceClosure callback) {
    start_loading_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure start_loading_callback_;
};

class VerticalTabViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  RootTabCollectionNode* root_node() {
    VerticalTabStripRegionView* region_view =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->vertical_tab_strip_region_view();
    return region_view->root_node_for_testing();
  }

  content::WebContents* AppendPinnedTab() {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->InsertWebContentsAt(
        browser()->tab_strip_model()->count(), std::move(contents),
        ADD_INHERIT_OPENER | ADD_ACTIVE | ADD_PINNED);
    return raw_contents;
  }
};

// TODO(crbug.com/464486134): All test flaky on Windows.
#if !BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, IconDataChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()->tab_strip_model(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The initial tab is the first child of the unpinned collection which is the
  // second child of the root node.
  TabCollectionNode* tab_node = root_node.children()[1]->children()[0].get();
  TabIcon* icon =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing())
          ->icon_for_testing();

  // Expect the favicon to be in the active state and not be loading initially.
  EXPECT_TRUE(icon->GetActiveStateForTesting());
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());

  // After changing network state, expect the favicon to be loading.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TestWebContentsObserver observer(web_contents);
  base::RunLoop run_loop;
  observer.SetStartLoadingCallback(run_loop.QuitClosure());
  browser()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL("/title1.html"), content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK, false),
      base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());

  // After adding a new tab, the old tab is no longer activated so the icon
  // should not be active.
  NavigateToURLWithDisposition(browser(), GURL(url::kAboutBlankURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_FALSE(icon->GetActiveStateForTesting());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, TitleDataChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()->tab_strip_model(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));
  views::Label* title =
      BrowserElementsViews::From(browser())->GetViewAs<views::Label>(
          kVerticalTabTitleElementId);

  // Expect the initial title to match the one in content/test/data/title2.html
  EXPECT_EQ(u"Title Of Awesomeness", title->GetText());

  // After navigating, expect title to be updated and match the one in
  // content/test/data/title3.html
  GURL changed_url = embedded_test_server()->GetURL("/title3.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), changed_url));
  EXPECT_EQ(u"Title Of More Awesomeness", title->GetText());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, AlertIndicatorDataChanged) {
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()->tab_strip_model(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The initial tab is the first child of the unpinned collection which is the
  // second child of the root node.
  TabCollectionNode* tab_node = root_node.children()[1]->children()[0].get();
  AlertIndicatorButton* alert_indicator =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing())
          ->alert_indicator_for_testing();

  // The alert indicator should not be visible initially.
  ASSERT_FALSE(alert_indicator->GetVisible());
  ASSERT_EQ(std::nullopt, alert_indicator->alert_state_for_testing());
  ASSERT_EQ(std::nullopt, alert_indicator->showing_alert_state());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // After changing the tab alert state, expect the indicator to be visible.
  base::ScopedClosureRunner scoped_closure_runner = web_contents->MarkAudible();
  web_contents->SetAudioMuted(false);
  browser()->tab_strip_model()->NotifyTabChanged(
      browser()->tab_strip_model()->GetActiveTab(), TabChangeType::kAll);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->alert_state_for_testing());
  EXPECT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->showing_alert_state());

  // After changing the tab alert, expect the indicator state to change.
  web_contents->SetAudioMuted(true);
  browser()->tab_strip_model()->NotifyTabChanged(
      browser()->tab_strip_model()->GetActiveTab(), TabChangeType::kAll);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(tabs::TabAlert::kAudioMuting,
            alert_indicator->alert_state_for_testing());
  EXPECT_EQ(tabs::TabAlert::kAudioMuting,
            alert_indicator->showing_alert_state());

  // After removing the tab alert, expect the indicator to still be visible
  // (because it is fading out).
  scoped_closure_runner.RunAndReset();
  // There is a 2 second hysteresis for the audible state, controlled by
  // RecentlyAudibleHelper. Fire the timer manually to remove the tab alert.
  RecentlyAudibleHelper* recently_audible_helper =
      RecentlyAudibleHelper::FromWebContents(web_contents);
  recently_audible_helper->SetNotRecentlyAudibleForTesting();
  recently_audible_helper->FireRecentlyAudibleTimerForTesting();
  browser()->tab_strip_model()->NotifyTabChanged(
      browser()->tab_strip_model()->GetActiveTab(), TabChangeType::kAll);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(std::nullopt, alert_indicator->alert_state_for_testing());
  EXPECT_EQ(tabs::TabAlert::kAudioMuting,
            alert_indicator->showing_alert_state());
}

// This test doesn't need the EnableTabMuting feature flag because it directly
// calls NotifyClick() on the button controller.
IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, AlertIndicatorMute) {
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()->tab_strip_model(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The initial tab is the first child of the unpinned collection which is the
  // second child of the root node.
  TabCollectionNode* tab_node = root_node.children()[1]->children()[0].get();
  AlertIndicatorButton* alert_indicator =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing())
          ->alert_indicator_for_testing();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner scoped_closure_runner = web_contents->MarkAudible();
  browser()->tab_strip_model()->NotifyTabChanged(
      browser()->tab_strip_model()->GetActiveTab(), TabChangeType::kAll);

  // Audio should be playing initially.
  ASSERT_TRUE(alert_indicator->GetVisible());
  ASSERT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->alert_state_for_testing());
  ASSERT_FALSE(web_contents->IsAudioMuted());

  // After clicking the alert indicator, audio should be muted.
  alert_indicator->button_controller()->NotifyClick();
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(tabs::TabAlert::kAudioMuting,
            alert_indicator->alert_state_for_testing());
  EXPECT_TRUE(web_contents->IsAudioMuted());

  // After clicking the alert indicator again, audio should no longer be muted.
  alert_indicator->button_controller()->NotifyClick();
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->alert_state_for_testing());
  EXPECT_FALSE(web_contents->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, CloseButtonDataChanged) {
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()->tab_strip_model(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The initial tab is the first child of the unpinned collection which is the
  // second child of the root node.
  TabCollectionNode* tab_node = root_node.children()[1]->children()[0].get();
  VerticalTabView* tab_view =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing());
  TabCloseButton* close_button = tab_view->close_button_for_testing();

  // Expect the close button to be showing initially.
  EXPECT_TRUE(close_button->GetVisible());

  // After adding a new tab, the old tab is no longer activated so the close
  // button should no longer be showing.
  NavigateToURLWithDisposition(browser(), GURL(url::kAboutBlankURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_FALSE(close_button->GetVisible());

  // After the mouse enters the tab, the close button should be showing.
  ui::MouseEvent mouse_entered_event(ui::EventType::kMouseEntered, gfx::Point(),
                                     gfx::Point(), base::TimeTicks(),
                                     ui::EF_NONE, ui::EF_NONE);
  tab_view->OnMouseEnteredForTesting(mouse_entered_event);
  EXPECT_TRUE(close_button->GetVisible());

  // After the mouse exits the tab, the close button should be hidden.
  ui::MouseEvent mouse_exited_event(ui::EventType::kMouseExited, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_NONE, ui::EF_NONE);
  tab_view->OnMouseExitedForTesting(mouse_exited_event);
  EXPECT_FALSE(close_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, CloseButtonPressed) {
  // Add a second tab.
  NavigateToURLWithDisposition(browser(), GURL(url::kAboutBlankURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()->tab_strip_model(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));
  root_node.SetController(vertical_tab_strip_controller());

  // The second tab is the second child of the unpinned collection which is the
  // second child of the root node.
  TabCollectionNode* tab_node = root_node.children()[1]->children()[1].get();
  VerticalTabView* tab_view =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing());
  TabCloseButton* close_button = tab_view->close_button_for_testing();
  ASSERT_TRUE(close_button->GetVisible());

  // Expect there to be two tabs initially.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // After pressing the close button, there should only be 1 tab remaining.
  close_button->button_controller()->NotifyClick();
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

// TODO(crbug.com/465540287): Determine how to test the background changing
// based on active/selected/hovered states.

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, PinnedTabsHideCloseButton) {
  AppendPinnedTab();

  // The initial tab is the first child of the pinned collection which is the
  // first child of the root node.
  TabCollectionNode* tab_node = root_node()->children()[0]->children()[0].get();
  VerticalTabView* tab =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing());

  // The favicon should be visible but the close button is not.
  EXPECT_TRUE(tab->icon_for_testing()->GetVisible());
  EXPECT_FALSE(tab->alert_indicator_for_testing()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, PinnedTabsRenderBorder) {
  AppendPinnedTab();

  // The initial tab is the first child of the pinned collection which is the
  // first child of the root node.
  TabCollectionNode* tab_node = root_node()->children()[0]->children()[0].get();
  VerticalTabView* tab =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing());

  EXPECT_TRUE(tab->GetBorder());

  // Unpin the tab.
  browser()->tab_strip_model()->SetTabPinned(0, false);

  EXPECT_FALSE(tab->GetBorder());
}

#endif  // !BUILDFLAG(IS_WIN)
