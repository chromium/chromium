// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/current_thread.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

class FakeTabDeclutterObserver : public TabDeclutterObserver {
 public:
  FakeTabDeclutterObserver() = default;

  void OnTriggerDeclutterUIVisibility() override {
    trigger_declutter_ui_visibility_count_++;
  }

  void OnUnusedTabsProcessed(std::vector<tabs::TabInterface*> stale_tabs,
                             std::map<GURL, std::vector<tabs::TabInterface*>>
                                 duplicate_tabs) override {
    unused_tabs_processed_count_++;
    processed_duplicate_tabs_ = duplicate_tabs;
    processed_stale_tabs_ = stale_tabs;
  }

  int unused_tabs_processed_count() const {
    return unused_tabs_processed_count_;
  }

  int trigger_declutter_ui_visibility_count() const {
    return trigger_declutter_ui_visibility_count_;
  }

  const std::vector<tabs::TabInterface*>& processed_stale_tabs() const {
    return processed_stale_tabs_;
  }

  const std::map<GURL, std::vector<tabs::TabInterface*>>&
  processed_duplicate_tabs() const {
    return processed_duplicate_tabs_;
  }

 private:
  int unused_tabs_processed_count_ = 0;
  int trigger_declutter_ui_visibility_count_ = 0;
  std::vector<tabs::TabInterface*> processed_stale_tabs_;
  std::map<GURL, std::vector<tabs::TabInterface*>> processed_duplicate_tabs_;
};

class TabDeclutterControllerBrowserTest : public InProcessBrowserTest {
 public:
  TabDeclutterControllerBrowserTest() {
    feature_list_.InitWithFeatures({features::kTabstripDeclutter}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetFocusedTabStripModelForTesting(browser()->tab_strip_model());

    // Setup the usage clock to be in session.
    if (!metrics::DesktopSessionDurationTracker::IsInitialized()) {
      metrics::DesktopSessionDurationTracker::Initialize();
    }

    auto* tracker = metrics::DesktopSessionDurationTracker::Get();
    tracker->OnVisibilityChanged(true, base::TimeDelta());
    tracker->OnUserEvent();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void AddTabsWithLastActiveTime(int num_tabs, int last_active_days) {
    for (int i = 0; i < num_tabs; ++i) {
      browser()->tab_strip_model()->AppendWebContents(
          CreateWebContents(base::Time::Now() - base::Days(last_active_days)),
          false);
    }
  }

  tabs::TabDeclutterController* tab_declutter_controller() {
    return browser()->browser_window_features()->tab_declutter_controller();
  }

  views::View* nudge_container() {
    if (features::HasTabSearchToolbarButton()) {
      return BrowserElementsViews::From(browser())->GetView(
          kTabStripActionContainerElementId);
    }

    return BrowserElementsViews::From(browser())->GetViewAs<TabSearchContainer>(
        kTabSearchContainerElementId);
  }

 protected:
  std::unique_ptr<content::WebContents> CreateWebContents(
      base::Time last_active_time) {
    content::WebContents::CreateParams create_params(browser()->profile());
    create_params.last_active_time = last_active_time;
    create_params.initially_hidden = true;
    return content::WebContents::Create(create_params);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestProcessInactiveTabs) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  // Add 4 tabs that are 8 days old. These are all stale.
  AddTabsWithLastActiveTime(4, 8);

  // Make one of the tabs pinned and another grouped.
  browser()->tab_strip_model()->SetTabPinned(1, true);
  browser()->tab_strip_model()->AddToNewGroup(std::vector<int>{2});

  task_runner->FastForwardBy(
      tab_declutter_controller()->declutter_timer_interval());

  EXPECT_EQ(fake_observer.unused_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);

  // Tabs at index 3 and 4 are stale tabs.
  std::vector<tabs::TabInterface*> expected_stale_tabs;
  expected_stale_tabs.push_back(browser()->tab_strip_model()->GetTabAtIndex(3));
  expected_stale_tabs.push_back(browser()->tab_strip_model()->GetTabAtIndex(4));

  EXPECT_EQ(fake_observer.processed_stale_tabs(), expected_stale_tabs);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestTriggerDeclutterUIWhenCriteriaMet) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  // Add 12 tabs that are 2 days old (not stale) and 4 tabs that are 8 days old
  // (stale).
  AddTabsWithLastActiveTime(12, 2);
  AddTabsWithLastActiveTime(4, 8);

  // Move forward in time to simulate declutter timer triggering.
  task_runner->FastForwardBy(
      tab_declutter_controller()->nudge_timer_interval());

  EXPECT_GE(fake_observer.unused_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 1);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestTriggerDeclutterUIWhenNoNewStaleTabsFound) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  // Add 12 tabs that are 2 days old (not stale) and 4 tabs that are 8 days old
  // (stale).
  AddTabsWithLastActiveTime(12, 2);
  AddTabsWithLastActiveTime(4, 8);

  // Move forward in time to simulate declutter timer triggering.
  task_runner->FastForwardBy(
      tab_declutter_controller()->nudge_timer_interval());

  EXPECT_GE(fake_observer.unused_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 1);

  // Move forward in time to simulate declutter timer triggering.
  task_runner->FastForwardBy(
      tab_declutter_controller()->nudge_timer_interval());
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 1);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestNudgeNotTriggeredWhenTabCountBelowThreshold) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  // Add 12 tabs that are 8 days old (stale).
  AddTabsWithLastActiveTime(12, 8);

  // Move forward in time to simulate declutter timer triggering.
  task_runner->FastForwardBy(
      tab_declutter_controller()->nudge_timer_interval());

  EXPECT_GE(fake_observer.unused_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestNudgeNotTriggeredWhenStaleTabCountBelowThreshold) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  // Add 14 tabs that are 2 days old (not stale) and 1 tab that are 8 days old
  // (stale).
  AddTabsWithLastActiveTime(14, 2);
  AddTabsWithLastActiveTime(1, 8);

  // Move forward in time to simulate declutter timer triggering.
  task_runner->FastForwardBy(
      tab_declutter_controller()->nudge_timer_interval());

  EXPECT_GE(fake_observer.unused_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestOnActionUIDismissedIncreasesTime) {
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  AddTabsWithLastActiveTime(12, 2);
  AddTabsWithLastActiveTime(4, 8);

  base::TimeDelta initial_nudge_interval =
      tab_declutter_controller()->nudge_timer_interval();

  // Move time forward by the initial nudge timer interval and check the next
  // valid nudge time.
  task_runner->FastForwardBy(initial_nudge_interval);
  nudge_container()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(initial_nudge_interval,
            tab_declutter_controller()->nudge_timer_interval());

  views::LabelButton* close_button;
  if (features::HasTabSearchToolbarButton()) {
    TabStripActionContainer* tab_strip_action_container =
        BrowserElementsViews::From(browser())
            ->GetViewAs<TabStripActionContainer>(
                kTabStripActionContainerElementId);
    EXPECT_TRUE(
        tab_strip_action_container->tab_declutter_button()->GetVisible());
    close_button = tab_strip_action_container->tab_declutter_button()
                       ->close_button_for_testing();
  } else {
    TabSearchContainer* tab_search_container =
        BrowserElementsViews::From(browser())->GetViewAs<TabSearchContainer>(
            kTabSearchContainerElementId);
    EXPECT_TRUE(tab_search_container->tab_declutter_button()->GetVisible());
    close_button = tab_search_container->tab_declutter_button()
                       ->close_button_for_testing();
  }

  // Click the close button.
  close_button->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  close_button->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));

  // After dismissal, the nudge interval should double.
  EXPECT_EQ(tab_declutter_controller()->nudge_timer_interval(),
            initial_nudge_interval * 2);
  EXPECT_EQ(tab_declutter_controller()->next_nudge_valid_time_ticks(),
            task_runner->GetMockTickClock()->NowTicks() +
                tab_declutter_controller()->nudge_timer_interval());
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest, TestDeclutterTabs) {
  // Add 12 tabs that are 2 days old (not stale) and 4 tabs that are 8 days old
  // (stale).
  AddTabsWithLastActiveTime(12, 2);
  AddTabsWithLastActiveTime(4, 8);

  std::vector<tabs::TabInterface*> stale_tabs =
      tab_declutter_controller()->GetStaleTabs();

  int initial_tab_count = browser()->tab_strip_model()->GetTabCount();
  int stale_tab_count = stale_tabs.size();
  tab_declutter_controller()->DeclutterTabs(stale_tabs, {});

  // Verify that the number of tabs has decreased by the number of stale tabs.
  int remaining_tab_count = browser()->tab_strip_model()->GetTabCount();
  EXPECT_EQ(remaining_tab_count, initial_tab_count - stale_tab_count);
}
IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestDoesNotDeclutterTabForAnotherBrowser) {
  Browser* browser_two =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::AddTabAt(browser_two, GURL(), -1, true);
  resource_coordinator::GetTabLifecycleUnitSource()
      ->SetFocusedTabStripModelForTesting(browser_two->tab_strip_model());

  // Add 12 tabs that are 2 days old (not stale) and 4 tabs that are 8 days old
  // (stale) in browser_one.
  AddTabsWithLastActiveTime(12, 2);
  AddTabsWithLastActiveTime(4, 8);

  std::vector<tabs::TabInterface*> stale_tabs =
      tab_declutter_controller()->GetStaleTabs();

  EXPECT_EQ(browser_two->tab_strip_model()->GetTabCount(), 1);
  browser_two->browser_window_features()
      ->tab_declutter_controller()
      ->DeclutterTabs(stale_tabs, {});

  // Verify that the number of tabs has not decreased in second browser.
  EXPECT_EQ(browser_two->tab_strip_model()->GetTabCount(), 1);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestExcludeTabsFromDeclutter) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  // Add 4 tabs that are 8 days old. These are all stale.
  AddTabsWithLastActiveTime(4, 8);

  tabs::TabInterface* tab_to_exclude =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  tab_declutter_controller()->ExcludeFromStaleTabs(tab_to_exclude);

  task_runner->FastForwardBy(
      tab_declutter_controller()->declutter_timer_interval());

  EXPECT_EQ(fake_observer.unused_tabs_processed_count(), 1);

  // Verify that the excluded tab is not part of the processed stale tabs.
  std::vector<tabs::TabInterface*> processed_stale_tabs =
      fake_observer.processed_stale_tabs();
  EXPECT_EQ(processed_stale_tabs.size(), 3ul);
  EXPECT_FALSE(std::find(processed_stale_tabs.begin(),
                         processed_stale_tabs.end(),
                         tab_to_exclude) != processed_stale_tabs.end());
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestInactiveBrowserDoesNotShowNudge) {
  // Activate another browser.
  Browser* const second_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(AddTabAtIndexToBrowser(second_browser, 0, GURL("about:blank"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));

  ui_test_utils::BrowserActivationWaiter browser_waiter(second_browser);
  second_browser->window()->Activate();
  browser_waiter.WaitForActivation();
  resource_coordinator::GetTabLifecycleUnitSource()
      ->SetFocusedTabStripModelForTesting(second_browser->tab_strip_model());

  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  // Add 4 tabs that are 8 days old. These are all stale.
  AddTabsWithLastActiveTime(4, 8);

  // Make one of the tabs pinned and another grouped.
  browser()->tab_strip_model()->SetTabPinned(1, true);
  browser()->tab_strip_model()->AddToNewGroup(std::vector<int>{2});

  task_runner->FastForwardBy(
      tab_declutter_controller()->nudge_timer_interval());

  EXPECT_GE(fake_observer.unused_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);

  // Reset focused_tab_strip_model_for_testing_ to eliminate reliance on browser
  // list ordering during destruction.
  resource_coordinator::GetTabLifecycleUnitSource()
      ->SetFocusedTabStripModelForTesting(nullptr);
}

class TabDeclutterControllerDuplicateTabsTest
    : public TabDeclutterControllerBrowserTest {
 public:
  TabDeclutterControllerDuplicateTabsTest() {
    feature_list_.InitWithFeatures(
        {features::kTabstripDeclutter, features::kTabstripDedupe}, {});
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    TabDeclutterControllerBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void AddDuplicateTabs(int num_tabs, const GURL url, int last_active_days) {
    AddTabsWithLastActiveTime(num_tabs, last_active_days);
    for (int i = 0; i < num_tabs; ++i) {
      content::WebContents* content =
          browser()
              ->tab_strip_model()
              ->GetTabAtIndex(browser()->tab_strip_model()->GetTabCount() - 1 -
                              i)
              ->GetContents();
      content::TestNavigationObserver observer(content);
      content->GetController().LoadURLWithParams(
          content::NavigationController::LoadURLParams(url));
      observer.WaitForNavigationFinished();
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerDuplicateTabsTest,
                       TestProcessDuplicateTabs) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  // Add 2 duplicate tab clusters.
  GURL duplicate_url_1(embedded_test_server()->GetURL("/links.html"));
  GURL duplicate_url_2(embedded_test_server()->GetURL("/title1.html"));

  AddDuplicateTabs(3, duplicate_url_1, 1);
  AddDuplicateTabs(2, duplicate_url_2, 1);

  tab_declutter_controller()->set_next_nudge_valid_time_ticks_for_testing(
      base::TimeTicks::Min());
  tab_declutter_controller()->GetDeclutterTimerForTesting()->user_task().Run();

  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 1);

  std::map<GURL, std::vector<tabs::TabInterface*>> processed_duplicates =
      fake_observer.processed_duplicate_tabs();

  EXPECT_EQ(processed_duplicates.count(duplicate_url_1), 1ul);
  EXPECT_EQ(processed_duplicates.count(duplicate_url_2), 1ul);
  EXPECT_EQ(processed_duplicates[duplicate_url_1].size(), 3ul);
  EXPECT_EQ(processed_duplicates[duplicate_url_2].size(), 2ul);

  tab_declutter_controller()->RemoveObserver(&fake_observer);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerDuplicateTabsTest,
                       TestProcessDuplicateTabsWithGroupedAndPinnedTabs) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  // Add 2 duplicate tab clusters.
  GURL duplicate_url_1(embedded_test_server()->GetURL("/links.html"));
  GURL duplicate_url_2(embedded_test_server()->GetURL("/title1.html"));

  AddDuplicateTabs(3, duplicate_url_1, 1);
  AddDuplicateTabs(2, duplicate_url_2, 1);

  browser()->tab_strip_model()->SetTabPinned(1, true);
  browser()->tab_strip_model()->AddToNewGroup({3});

  tab_declutter_controller()->set_next_nudge_valid_time_ticks_for_testing(
      base::TimeTicks::Min());
  tab_declutter_controller()->GetDeclutterTimerForTesting()->user_task().Run();

  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);

  std::map<GURL, std::vector<tabs::TabInterface*>> processed_duplicates =
      fake_observer.processed_duplicate_tabs();

  EXPECT_EQ(processed_duplicates.count(duplicate_url_1), 0ul);

  EXPECT_EQ(processed_duplicates.count(duplicate_url_2), 1ul);
  EXPECT_EQ(processed_duplicates[duplicate_url_2].size(), 2ul);

  tab_declutter_controller()->RemoveObserver(&fake_observer);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerDuplicateTabsTest,
                       TestExcludeTabsFromDedupe) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller()->AddObserver(&fake_observer);

  // Add 2 duplicate tab clusters.
  GURL duplicate_url_1(embedded_test_server()->GetURL("/links.html"));
  GURL duplicate_url_2(embedded_test_server()->GetURL("/title1.html"));

  AddDuplicateTabs(3, duplicate_url_1, 1);
  AddDuplicateTabs(2, duplicate_url_2, 1);

  tab_declutter_controller()->ExcludeFromDuplicateTabs(duplicate_url_1);

  tab_declutter_controller()->set_next_nudge_valid_time_ticks_for_testing(
      base::TimeTicks::Min());
  tab_declutter_controller()->GetDeclutterTimerForTesting()->user_task().Run();

  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);

  std::map<GURL, std::vector<tabs::TabInterface*>> processed_duplicates =
      fake_observer.processed_duplicate_tabs();

  EXPECT_EQ(processed_duplicates.count(duplicate_url_1), 0ul);
  EXPECT_EQ(processed_duplicates.count(duplicate_url_2), 1ul);
  EXPECT_EQ(processed_duplicates[duplicate_url_2].size(), 2ul);
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerDuplicateTabsTest,
                       TestDeclutterTabs) {
  // Add 12 tabs that are 2 days old (not stale) and 4 tabs that are 8 days old
  // (stale).
  AddTabsWithLastActiveTime(12, 2);
  AddTabsWithLastActiveTime(4, 8);

  // Add 2 duplicate tab clusters.
  GURL duplicate_url_1(embedded_test_server()->GetURL("/links.html"));
  GURL duplicate_url_2(embedded_test_server()->GetURL("/title1.html"));

  AddDuplicateTabs(3, duplicate_url_1, 1);
  AddDuplicateTabs(2, duplicate_url_2, 1);

  std::vector<tabs::TabInterface*> stale_tabs =
      tab_declutter_controller()->GetStaleTabs();

  int initial_tab_count = browser()->tab_strip_model()->GetTabCount();
  int stale_tab_count = stale_tabs.size();
  tab_declutter_controller()->DeclutterTabs(stale_tabs, {duplicate_url_1});

  // Verify that the number of tabs has decreased by the number of stale tabs.
  int remaining_tab_count = browser()->tab_strip_model()->GetTabCount();
  EXPECT_EQ(remaining_tab_count, initial_tab_count - (stale_tab_count + 2));
}
