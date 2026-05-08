// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab/tab_icon.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/tabs/public/tab_alert.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget_utils.h"

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

// This class observes TabStripModel for the first new tab that is added, then
// observes the changes to that new tab's title.
class NewTabTitleObserver : public TabStripModelObserver {
 public:
  explicit NewTabTitleObserver(
      TabStripModel* tab_strip_model,
      RootTabCollectionNode* root_node,
      base::RepeatingCallback<void(std::u16string_view)> title_changed_callback)
      : tab_strip_model_(tab_strip_model),
        root_node_(root_node),
        title_changed_callback_(title_changed_callback) {
    tab_strip_model_->AddObserver(this);
  }
  ~NewTabTitleObserver() override { tab_strip_model_->RemoveObserver(this); }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      const auto& insert = *change.GetInsert();
      const auto& content = insert.contents[0];
      TabCollectionNode* tab_node =
          root_node_->children()[1]->children()[content.index].get();
      VerticalTabView* tab_view =
          views::AsViewClass<VerticalTabView>(tab_node->view());
      views::Label* title = views::AsViewClass<views::Label>(
          tab_view->GetViewByElementId(kVerticalTabTitleElementId));
      views::PropertyChangedCallback callback =
          base::BindRepeating(
              [](views::Label* title) { return title->GetText(); }, title)
              .Then(title_changed_callback_);
      callback.Run();
      text_changed_subscription_ = title->AddTextChangedCallback(callback);
      tab_strip_model_->RemoveObserver(this);
    }
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
  raw_ptr<RootTabCollectionNode> root_node_;
  base::RepeatingCallback<void(std::u16string_view)> title_changed_callback_;
  base::CallbackListSubscription text_changed_subscription_;
};

class VerticalTabViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  void WaitForLayout(views::View* view) {
    ASSERT_TRUE(base::test::RunUntil([&]() { return !view->needs_layout(); }));
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, IconDataChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* icon = BrowserElementsViews::From(browser())->GetViewAs<TabIcon>(
      kTabIconElementId);

  // Expect the favicon to be in the active state and not be loading initially.
  ASSERT_TRUE(icon->GetActiveStateForTesting());
  ASSERT_FALSE(icon->GetShowingLoadingAnimation());
  ASSERT_FALSE(icon->GetShowingAttentionIndicator());
  ASSERT_FALSE(icon->GetShowingDiscardIndicator());

  // After changing network state, expect the favicon to be loading.
  content::WebContents* web_contents =
      tab_strip_model()->GetActiveWebContents();
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

  // After setting the tab as blocked, expect the attention indicator to not be
  // showing because the tab is active.
  tab_strip_model()->SetTabBlocked(0, true);
  EXPECT_FALSE(icon->GetShowingAttentionIndicator());

  // After adding a new tab, the old tab is no longer activated so the icon
  // should not be active, and the attention indicator should be showing.
  AppendTab();
  EXPECT_FALSE(icon->GetActiveStateForTesting());
  EXPECT_TRUE(icon->GetShowingAttentionIndicator());

  // After setting the tab as not blocked, expect the attention indicator to not
  // be showing.
  tab_strip_model()->SetTabBlocked(0, false);
  EXPECT_FALSE(icon->GetShowingAttentionIndicator());

  // After setting the tab as needing attention, expect the attention indicator
  // to be showing.
  tab_strip_model()->SetTabNeedsAttentionAt(0, true);
  EXPECT_TRUE(icon->GetShowingAttentionIndicator());

  // After discarding the tab, the icon should show the discard indicator.
  tabs::TabInterface* tab = tab_strip_model()->GetTabAtIndex(0);
  performance_manager::user_tuning::UserPerformanceTuningManager::GetInstance()
      ->DiscardPageForTesting(tab->GetContents());
  EXPECT_TRUE(icon->GetShowingDiscardIndicator());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, TitleDataChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

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

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, TitleLoading) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = embedded_test_server()->GetURL("/link_new_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Open a link to a new page. Expect that the title shows a loading
  // placeholder
#if BUILDFLAG(IS_MAC)
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
#else
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
#endif
  base::RunLoop run_loop;
  NewTabTitleObserver observer(
      browser()->tab_strip_model(), root_node(),
      base::BindRepeating(
          [&](base::RepeatingClosure quit_closure,
              std::u16string expected_title, std::u16string_view title) {
            if (title == expected_title) {
              quit_closure.Run();
            }
          },
          run_loop.QuitClosure(), expected_title));
  ASSERT_TRUE(
      ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
             "setTimeout(() => "
             "document.getElementById('new-page-link').click(), 1000);"));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, AlertIndicatorDataChanged) {
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  auto* alert_indicator =
      BrowserElementsViews::From(browser())->GetViewAs<AlertIndicatorButton>(
          kTabAlertIndicatorButtonElementId);

  // The alert indicator should not be visible initially.
  ASSERT_FALSE(alert_indicator->GetVisible());
  ASSERT_EQ(std::nullopt, alert_indicator->alert_state_for_testing());
  ASSERT_EQ(std::nullopt, alert_indicator->showing_alert_state());

  content::WebContents* web_contents =
      tab_strip_model()->GetActiveWebContents();

  // After changing the tab alert state, expect the indicator to be visible.
  base::ScopedClosureRunner scoped_closure_runner = web_contents->MarkAudible();
  web_contents->SetAudioMuted(false);
  tab_strip_model()->NotifyTabChanged(tab_strip_model()->GetActiveTab(),
                                      TabChangeType::kAll);
  WaitForLayout(tab_view);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->alert_state_for_testing());
  EXPECT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->showing_alert_state());

  // After changing the tab alert, expect the indicator state to change.
  web_contents->SetAudioMuted(true);
  tab_strip_model()->NotifyTabChanged(tab_strip_model()->GetActiveTab(),
                                      TabChangeType::kAll);
  WaitForLayout(tab_view);
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
  tab_strip_model()->NotifyTabChanged(tab_strip_model()->GetActiveTab(),
                                      TabChangeType::kAll);
  WaitForLayout(tab_view);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(std::nullopt, alert_indicator->alert_state_for_testing());
  EXPECT_EQ(tabs::TabAlert::kAudioMuting,
            alert_indicator->showing_alert_state());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, AlertIndicatorMuteEnabled) {
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  auto* alert_indicator =
      BrowserElementsViews::From(browser())->GetViewAs<AlertIndicatorButton>(
          kTabAlertIndicatorButtonElementId);

  content::WebContents* web_contents =
      tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner scoped_closure_runner = web_contents->MarkAudible();
  tab_strip_model()->NotifyTabChanged(tab_strip_model()->GetActiveTab(),
                                      TabChangeType::kAll);

  // The audio playing indicator should be visible but not enabled, because the
  // flag kEnableTabMuting is off.
  WaitForLayout(tab_view);
  ASSERT_TRUE(alert_indicator->GetVisible());
  ASSERT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->alert_state_for_testing());
  ASSERT_FALSE(alert_indicator->GetEnabled());
}

class VerticalTabViewTabMutingEnabledTest : public VerticalTabViewTest {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{tabs::kVerticalTabs, {}}, {media::kEnableTabMuting, {}}};
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabViewTabMutingEnabledTest,
                       AlertIndicatorMuteEnabled) {
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  auto* alert_indicator =
      BrowserElementsViews::From(browser())->GetViewAs<AlertIndicatorButton>(
          kTabAlertIndicatorButtonElementId);

  content::WebContents* web_contents =
      tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner scoped_closure_runner = web_contents->MarkAudible();
  tab_strip_model()->NotifyTabChanged(tab_strip_model()->GetActiveTab(),
                                      TabChangeType::kAll);

  // Normally tab width would be a function of the size of the vertical tab
  // strip and whether the tab is in a split/group. To make this test not depend
  // on the layouts of those classes, manually set the bounds of the tab.
  auto set_tab_width = [&](int width) {
    tab_view->SetBounds(tab_view->x(), tab_view->y(), width,
                        tab_view->height());
  };
  constexpr int kEnabledWidth = 128;
  constexpr int kDisabledWidth = 48;

  // Initially the audio playing indicator should be visible and enabled.
  set_tab_width(kEnabledWidth);
  ASSERT_TRUE(alert_indicator->GetVisible());
  ASSERT_TRUE(alert_indicator->GetEnabled());

  // Add a second tab. The alert indicator should still be enabled.
  AppendTab();
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_TRUE(alert_indicator->GetEnabled());

  // Resize the original tab so that there is not enough width outside of the
  // alert indicator to activate it. The alert indicator should be disabled.
  set_tab_width(kDisabledWidth);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_FALSE(alert_indicator->GetEnabled());

  // Activate the original tab. The alert indicator should be enabled again.
  tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_TRUE(alert_indicator->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTabMutingEnabledTest,
                       AlertIndicatorMuteToggle) {
  base::HistogramTester histogram_tester;
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  auto* alert_indicator =
      BrowserElementsViews::From(browser())->GetViewAs<AlertIndicatorButton>(
          kTabAlertIndicatorButtonElementId);

  content::WebContents* web_contents =
      tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner scoped_closure_runner = web_contents->MarkAudible();
  tab_strip_model()->NotifyTabChanged(tab_strip_model()->GetActiveTab(),
                                      TabChangeType::kAll);

  // Audio should be playing initially.
  WaitForLayout(tab_view);
  ASSERT_TRUE(alert_indicator->GetVisible());
  ASSERT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->alert_state_for_testing());
  ASSERT_FALSE(web_contents->IsAudioMuted());
  ASSERT_EQ(0,
            histogram_tester.GetBucketCount("Media.Audio.TabAudioMuted", true));
  ASSERT_EQ(
      0, histogram_tester.GetBucketCount("Media.Audio.TabAudioMuted", false));

  // After clicking the alert indicator, audio should be muted.
  ASSERT_TRUE(alert_indicator->GetEnabled());
  alert_indicator->button_controller()->NotifyClick();
  WaitForLayout(tab_view);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(tabs::TabAlert::kAudioMuting,
            alert_indicator->alert_state_for_testing());
  EXPECT_TRUE(web_contents->IsAudioMuted());
  histogram_tester.ExpectBucketCount("Media.Audio.TabAudioMuted", true, 1);
  histogram_tester.ExpectBucketCount("Media.Audio.TabAudioMuted", false, 0);

  // After clicking the alert indicator again, audio should no longer be muted.
  ASSERT_TRUE(alert_indicator->GetEnabled());
  alert_indicator->button_controller()->NotifyClick();
  WaitForLayout(tab_view);
  EXPECT_TRUE(alert_indicator->GetVisible());
  EXPECT_EQ(tabs::TabAlert::kAudioPlaying,
            alert_indicator->alert_state_for_testing());
  EXPECT_FALSE(web_contents->IsAudioMuted());
  histogram_tester.ExpectBucketCount("Media.Audio.TabAudioMuted", true, 1);
  histogram_tester.ExpectBucketCount("Media.Audio.TabAudioMuted", false, 1);
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, CloseButtonVisibilityActiveTab) {
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  TabCloseButton* close_button = tab_view->close_button_for_testing();

  // Expect the close button to be showing initially.
  EXPECT_TRUE(close_button->GetVisible());

  // After adding a new tab, the old tab is no longer activated so the close
  // button should no longer be showing.
  AppendTab();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !close_button->GetVisible(); }));
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, CloseButtonVisibilityHover) {
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  TabCloseButton* close_button = tab_view->close_button_for_testing();

  // Deactivate the tab.
  AppendTab();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !close_button->GetVisible(); }));

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(browser()->GetBrowserView().GetWidget()),
      browser()->GetBrowserView().GetNativeWindow());

  // After the mouse enters the tab, the close button should be showing.
  event_generator.MoveMouseTo(tab_view->GetBoundsInScreen().CenterPoint());
  WaitForLayout(tab_view);
  EXPECT_TRUE(close_button->GetVisible());

  // After the mouse exits the tab, the close button should be hidden.
  event_generator.MoveMouseTo(gfx::Point());
  WaitForLayout(tab_view);
  EXPECT_FALSE(close_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, CloseButtonVisibilityCollapsed) {
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  TabCloseButton* close_button = tab_view->close_button_for_testing();

  // Deactivate the tab.
  AppendTab();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !close_button->GetVisible(); }));

  // Collapse the tab strip.
  tabs::VerticalTabStripStateController::From(browser())->RequestCollapse(true);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !BrowserAnimationController::From(browser())->IsAnimating(
        TabStripAnimations::kVerticalTabStrip);
  }));

  // After collapsing the tab strip, the close button should be hidden because
  // the tab is not hovered.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !close_button->GetVisible(); }));

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(browser()->GetBrowserView().GetWidget()),
      browser()->GetBrowserView().GetNativeWindow());

  // After the mouse enters the tab, the close button should still be hidden
  // because the tab is not active.
  event_generator.MoveMouseTo(tab_view->GetBoundsInScreen().CenterPoint());
  WaitForLayout(tab_view);
  EXPECT_FALSE(close_button->GetVisible());

  // After the mouse exits the tab, the close button should be hidden.
  event_generator.MoveMouseTo(gfx::Point());
  WaitForLayout(tab_view);
  EXPECT_FALSE(close_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest,
                       CloseButtonVisibilityActiveCollapsed) {
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  TabCloseButton* close_button = tab_view->close_button_for_testing();

  // Collapse the tab strip.
  tabs::VerticalTabStripStateController::From(browser())->RequestCollapse(true);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !BrowserAnimationController::From(browser())->IsAnimating(
        TabStripAnimations::kVerticalTabStrip);
  }));

  // Now that the tab is collapsed the close button should be hidden.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !close_button->GetVisible(); }));

  // Hovering the active tab while collapsed should show the close button.
  // TODO(crbug.com/492603554): Investigate why using EventGenerator here
  // flakes.
  tab_view->UpdateHovered(true);
  WaitForLayout(tab_view);
  EXPECT_TRUE(close_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, CloseButtonPressed) {
  // Add a second tab.
  AppendTab();

  // The second tab is the second child of the unpinned collection.
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[1].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  TabCloseButton* close_button = tab_view->close_button_for_testing();
  ASSERT_TRUE(close_button->GetVisible());

  // Expect there to be two tabs initially.
  ASSERT_EQ(2, tab_strip_model()->count());

  // After pressing the close button, there should only be 1 tab remaining.
  close_button->button_controller()->NotifyClick();
  EXPECT_EQ(1, tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, RenderInactiveWhenClosing) {
  // Add a second tab.
  AppendTab();
  ASSERT_EQ(2, tab_strip_model()->count());

  // The second tab is active.
  TabCollectionNode* tab_node = unpinned_collection_node()->children()[1].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());
  tab_view->UpdateHovered(true);

  ASSERT_TRUE(tab_view->IsActive());
  EXPECT_TRUE(tab_view->IsHoverAnimationActive());

  // Close the tab.
  tab_strip_model()->CloseWebContentsAt(1, TabCloseTypes::CLOSE_USER_GESTURE);

  // The tab_view should now be inactive despite being the active tab
  // just before closing.
  EXPECT_FALSE(tab_view->IsActive());
  EXPECT_TRUE(tab_view->IsHoverAnimationActive());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, PinnedTabsHideCloseButton) {
  AppendPinnedTab();

  // The initial tab is the first child of the pinned collection.
  TabCollectionNode* tab_node = pinned_collection_node()->children()[0].get();
  VerticalTabView* tab = views::AsViewClass<VerticalTabView>(tab_node->view());

  // The favicon should be visible but the close button is not.
  EXPECT_TRUE(tab->GetViewByElementId(kTabIconElementId)->GetVisible());
  EXPECT_FALSE(
      tab->GetViewByElementId(kTabAlertIndicatorButtonElementId)->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, PinnedTabsRenderBorder) {
  AppendPinnedTab();

  // The initial tab is the first child of the pinned collection.
  VerticalTabView* pinned_tab = views::AsViewClass<VerticalTabView>(
      pinned_collection_node()->children()[0].get()->view());

  EXPECT_TRUE(pinned_tab->GetBorder());

  // Unpin the tab.
  tab_strip_model()->SetTabPinned(0, false);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return unpinned_collection_node()->children().size() == 2u; }));

  // The first child of the unpinned collection is the tab that has been
  // unpinned.
  VerticalTabView* unpinned_tab = views::AsViewClass<VerticalTabView>(
      unpinned_collection_node()->children()[0].get()->view());

  EXPECT_FALSE(unpinned_tab->GetBorder());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, LogsTabCloseMetrics) {
  base::UserActionTester user_action_tester;

  AppendTab();
  TabCollectionNode* tab_node = unpinned_collection_node()->GetNodeForHandle(
      tab_strip_model()->GetActiveTab()->GetHandle());
  TabCloseButton* close_button =
      views::AsViewClass<VerticalTabView>(tab_node->view())
          ->close_button_for_testing();
  ASSERT_TRUE(close_button->GetVisible());

  close_button->button_controller()->NotifyClick();

  EXPECT_EQ(user_action_tester.GetActionCount("CloseTab_NoAlertIndicator"), 1);
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest,
                       LogsTabCloseMetrics_AudioIndicator) {
  base::UserActionTester user_action_tester;

  AppendTab();
  RecentlyAudibleHelper::FromWebContents(tab_strip_model()->GetWebContentsAt(0))
      ->SetCurrentlyAudibleForTesting();
  tab_strip_model()->ActivateTabAt(0);

  TabCollectionNode* tab_node = unpinned_collection_node()->GetNodeForHandle(
      tab_strip_model()->GetActiveTab()->GetHandle());
  TabCloseButton* close_button =
      views::AsViewClass<VerticalTabView>(tab_node->view())
          ->close_button_for_testing();
  ASSERT_TRUE(close_button->GetVisible());

  close_button->button_controller()->NotifyClick();

  EXPECT_EQ(user_action_tester.GetActionCount("CloseTab_AudioIndicator"), 1);
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest,
                       LogsTabCloseMetrics_RecordingIndicator) {
  base::UserActionTester user_action_tester;

  AppendTab();
  blink::mojom::StreamDevices devices;
  blink::MediaStreamDevice video_device = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_media_device",
      "fake_media_device");
  devices.video_device = video_device;
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();

  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          tab_strip_model()->GetWebContentsAt(0), devices);
  video_stream_ui->OnStarted(
      base::RepeatingClosure(), content::MediaStreamUI::SourceCallback(),
      /*label=*/std::string(),
      /*screen_capture_ids=*/{}, content::MediaStreamUI::StateChangeCallback());
  tab_strip_model()->ActivateTabAt(0);

  TabCollectionNode* tab_node = unpinned_collection_node()->GetNodeForHandle(
      tab_strip_model()->GetActiveTab()->GetHandle());
  TabCloseButton* close_button =
      views::AsViewClass<VerticalTabView>(tab_node->view())
          ->close_button_for_testing();
  ASSERT_TRUE(close_button->GetVisible());

  close_button->button_controller()->NotifyClick();

  EXPECT_EQ(user_action_tester.GetActionCount("CloseTab_RecordingIndicator"),
            1);
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, LogsTabCloseMetrics_SplitView) {
  base::UserActionTester user_action_tester;

  AppendSplitTab();
  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
          ->GetNodeForHandle(tab_strip_model()->GetActiveTab()->GetHandle());
  TabCloseButton* close_button =
      views::AsViewClass<VerticalTabView>(tab_node->view())
          ->close_button_for_testing();
  ASSERT_TRUE(close_button->GetVisible());

  close_button->button_controller()->NotifyClick();

  EXPECT_EQ(user_action_tester.GetActionCount("CloseTab_NoAlertIndicator"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("CloseTab_StartTabInSplit"), 1);

  user_action_tester.ResetCounts();

  AppendSplitTab();
  tab_node = unpinned_collection_node()
                 ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                 ->GetNodeForHandle(
                     tab_strip_model()
                         ->GetTabAtIndex(tab_strip_model()->active_index() + 1)
                         ->GetHandle());
  close_button = views::AsViewClass<VerticalTabView>(tab_node->view())
                     ->close_button_for_testing();
  WaitForLayout(tab_node->view());
  ASSERT_TRUE(close_button->GetVisible());

  close_button->button_controller()->NotifyClick();

  EXPECT_EQ(user_action_tester.GetActionCount("CloseTab_NoAlertIndicator"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("CloseTab_EndTabInSplit"), 1);
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest,
                       SplitTabsAnchorHoverCardToParentSplitViewContainer) {
  AppendSplitTab();

  // Get the view for both tabs in the split.
  // Note: tab at index 0 is a normal tab.
  tabs::TabInterface* tab_1 = tab_strip_model()->GetTabAtIndex(1);
  tabs::TabInterface* tab_2 = tab_strip_model()->GetTabAtIndex(2);

  TabCollectionNode* split_tab_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::SPLIT);
  VerticalSplitTabView* split_tab_view =
      views::AsViewClass<VerticalSplitTabView>(split_tab_node->view());

  VerticalTabView* tab_1_view = views::AsViewClass<VerticalTabView>(
      split_tab_node->GetNodeForHandle(tab_1->GetHandle())->view());
  ASSERT_EQ(tab_1_view->GetAnchor().GetIfView(), split_tab_view);

  VerticalTabView* tab_2_view = views::AsViewClass<VerticalTabView>(
      split_tab_node->GetNodeForHandle(tab_2->GetHandle())->view());
  ASSERT_EQ(tab_2_view->GetAnchor().GetIfView(), split_tab_view);
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest,
                       HoverCardControllerSetsCorrectAnchorTarget) {
  AppendSplitTab();
  // Get the view for the non split tab, and both tabs in the split.
  // Tab at 0 is the ordinary non-split tab.
  tabs::TabInterface* tab_0 = tab_strip_model()->GetTabAtIndex(0);
  tabs::TabInterface* tab_1 = tab_strip_model()->GetTabAtIndex(1);
  tabs::TabInterface* tab_2 = tab_strip_model()->GetTabAtIndex(2);

  VerticalTabView* tab_0_target = AsViewClass<VerticalTabView>(
      unpinned_collection_node()->GetNodeForHandle(tab_0->GetHandle())->view());

  TabCollectionNode* split_tab_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::SPLIT);

  VerticalTabView* tab_1_target = AsViewClass<VerticalTabView>(
      split_tab_node->GetNodeForHandle(tab_1->GetHandle())->view());

  VerticalTabView* tab_2_target = AsViewClass<VerticalTabView>(
      split_tab_node->GetNodeForHandle(tab_2->GetHandle())->view());

  // Show hover card for each tab and then check that we set the
  // target tab is set correctly.
  hover_card_controller()->UpdateHoverCard(
      tab_0_target, TabSlotController::HoverCardUpdateType::kHover);
  EXPECT_EQ(tab_0_target, hover_card_controller()->target_tab());

  hover_card_controller()->UpdateHoverCard(
      tab_1_target, TabSlotController::HoverCardUpdateType::kHover);
  EXPECT_EQ(tab_1_target, hover_card_controller()->target_tab());

  hover_card_controller()->UpdateHoverCard(
      tab_2_target, TabSlotController::HoverCardUpdateType::kHover);
  EXPECT_EQ(tab_2_target, hover_card_controller()->target_tab());

  hover_card_controller()->UpdateHoverCard(
      nullptr, TabSlotController::HoverCardUpdateType::kTabRemoved);
  EXPECT_EQ(nullptr, hover_card_controller()->target_tab());
}

class VerticalTabViewDataSharingEnabledTest : public VerticalTabViewTest {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{tabs::kVerticalTabs, {}},
            {data_sharing::features::kDataSharingFeature, {}}};
  }

 private:
  // Disable animations so that tabs can be immediately clicked after being
  // added to a group.
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

IN_PROC_BROWSER_TEST_F(VerticalTabViewDataSharingEnabledTest,
                       LogsTabSwitchMetrics) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  // Add the initial tab to a group, then add a new tab.
  tab_strip_model()->AddToNewGroup({0});
  AppendTab();

  // Get the view for the initial tab that is in a group.
  tabs::TabInterface* tab = tab_strip_model()->GetTabAtIndex(0);
  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::GROUP)
          ->GetNodeForHandle(tab->GetHandle());
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());

  // Ensure that the group is a saved tab group.
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(GetProfile());
  tab_group_service->MakeTabGroupSharedForTesting(
      tab->GetGroup().value(), syncer::CollaborationId("collaboration_id"));

  ASSERT_EQ(1, tab_strip_model()->active_index());
  ASSERT_EQ(
      0, histogram_tester.GetBucketCount("TabStrip.Tab.Views.ActivationAction",
                                         TabActivationTypes::kTab));
  ASSERT_EQ(0, user_action_tester.GetActionCount("TabGroups_SwitchGroupedTab"));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "TabGroups.Shared.SwitchGroupedTab"));
  ASSERT_EQ(0, user_action_tester.GetActionCount("SwitchTab_Click"));

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(browser()->GetBrowserView().GetWidget()),
      browser()->GetBrowserView().GetNativeWindow());
  event_generator.MoveMouseTo(tab_view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  EXPECT_EQ(0, tab_strip_model()->active_index());
  histogram_tester.ExpectBucketCount("TabStrip.Tab.Views.ActivationAction",
                                     TabActivationTypes::kTab, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount("TabGroups_SwitchGroupedTab"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TabGroups.Shared.SwitchGroupedTab"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("SwitchTab_Click"));
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, AlertIndicatorDecorateOnCollapse) {
  tabs::VerticalTabStripStateController::From(browser())->RequestCollapse(true);

  // Wait for the collapse animation to finish and ensure the width reaches
  // kCollapsedWidth.
  VerticalTabStripRegionView* const region_view =
      browser()->GetBrowserView().vertical_tab_strip_region_view_for_testing();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !BrowserAnimationController::From(browser())->IsAnimating(
               TabStripAnimations::kVerticalTabStrip) &&
           region_view->width() <= VerticalTabStripRegionView::kCollapsedWidth;
  }));

  TabCollectionNode* tab_node = unpinned_collection_node()->children()[0].get();
  VerticalTabView* tab_view =
      views::AsViewClass<VerticalTabView>(tab_node->view());

  // Ensure the tab is active and hovered so that the close button is visible.
  tab_strip_model()->ActivateTabAt(0);
  // TODO(crbug.com/492603554): Investigate why using EventGenerator here
  // flakes.
  tab_view->UpdateHovered(true);

  auto* alert_indicator =
      tab_view->GetViewByElementId(kTabAlertIndicatorButtonElementId);

  // Set alert state to audio playing.
  content::WebContents* web_contents =
      tab_strip_model()->GetActiveWebContents();
  base::ScopedClosureRunner scoped_closure_runner = web_contents->MarkAudible();
  tab_strip_model()->NotifyTabChanged(tab_strip_model()->GetTabAtIndex(0),
                                      TabChangeType::kAll);
  WaitForLayout(tab_view);

  // In collapsed mode, the active and hovered tab shows the close button as the
  // first visible child. The alert indicator is the second visible child, so it
  // should be hidden from normal rendering and instead trigger the
  // decorate_on_collapse logic.
  const gfx::Rect tab_bounds = tab_view->GetLocalBounds();
  const gfx::Rect expected_bounds(tab_bounds.width() / 2,
                                  tab_bounds.height() / 2, 0, 0);
  EXPECT_EQ(expected_bounds, alert_indicator->bounds());
}
