// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/base_event_utils.h"

class FakeTabDeclutterObserver : public TabDeclutterObserver {
 public:
  FakeTabDeclutterObserver() = default;

  void OnStaleTabsProcessed(const std::vector<tabs::TabModel*> tabs) override {
    stale_tabs_processed_count_++;
    processed_tabs_ = tabs;
  }

  void OnTriggerDeclutterUIVisibility(bool visible) override {
    trigger_declutter_ui_visibility_count_++;
    ui_visibility_ = visible;
  }

  int stale_tabs_processed_count() const { return stale_tabs_processed_count_; }

  int trigger_declutter_ui_visibility_count() const {
    return trigger_declutter_ui_visibility_count_;
  }

  const std::vector<tabs::TabModel*>& processed_tabs() const {
    return processed_tabs_;
  }

  bool ui_visibility() const { return ui_visibility_; }

 private:
  int stale_tabs_processed_count_ = 0;
  int trigger_declutter_ui_visibility_count_ = 0;
  std::vector<tabs::TabModel*> processed_tabs_;
  bool ui_visibility_;
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

  TabSearchContainer* tab_search_container() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->tab_strip_region_view()
        ->tab_search_container();
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

  EXPECT_EQ(fake_observer.stale_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);

  // Tabs at index 3 and 4 are stale tabs.
  std::vector<tabs::TabModel*> expected_stale_tabs;
  expected_stale_tabs.push_back(browser()->tab_strip_model()->GetTabAtIndex(3));
  expected_stale_tabs.push_back(browser()->tab_strip_model()->GetTabAtIndex(4));

  EXPECT_EQ(fake_observer.processed_tabs(), expected_stale_tabs);
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

  EXPECT_GE(fake_observer.stale_tabs_processed_count(), 1);
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

  EXPECT_GE(fake_observer.stale_tabs_processed_count(), 1);
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

  EXPECT_GE(fake_observer.stale_tabs_processed_count(), 1);
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

  EXPECT_GE(fake_observer.stale_tabs_processed_count(), 1);
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
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(initial_nudge_interval,
            tab_declutter_controller()->nudge_timer_interval());

  TabSearchContainer* tab_search_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->tab_strip_region_view()
          ->tab_search_container();
  EXPECT_TRUE(tab_search_container->tab_declutter_button()->GetVisible());
  views::LabelButton* close_button =
      tab_search_container->tab_declutter_button()->close_button_for_testing();

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

  std::vector<tabs::TabModel*> stale_tabs =
      tab_declutter_controller()->GetStaleTabs();

  int initial_tab_count = browser()->tab_strip_model()->GetTabCount();
  int stale_tab_count = stale_tabs.size();
  tab_declutter_controller()->DeclutterTabs(stale_tabs);

  // Verify that the number of tabs has decreased by the number of stale tabs.
  int remaining_tab_count = browser()->tab_strip_model()->GetTabCount();
  EXPECT_EQ(remaining_tab_count, initial_tab_count - stale_tab_count);
}

// TODO(b/369681212): Enable test after fixing Linux ASan LSan errors.
IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       DISABLED_TestDoesNotDeclutterTabForAnotherBrowser) {
  Browser* browser_two =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::AddTabAt(browser_two, GURL(), -1, true);

  // Add 12 tabs that are 2 days old (not stale) and 4 tabs that are 8 days old
  // (stale) in browser_one.
  AddTabsWithLastActiveTime(12, 2);
  AddTabsWithLastActiveTime(4, 8);

  std::vector<tabs::TabModel*> stale_tabs =
      tab_declutter_controller()->GetStaleTabs();

  EXPECT_EQ(browser_two->tab_strip_model()->GetTabCount(), 1);
  browser_two->browser_window_features()
      ->tab_declutter_controller()
      ->DeclutterTabs(stale_tabs);

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

  tabs::TabModel* tab_to_exclude =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  tab_declutter_controller()->ExcludeFromStaleTabs(tab_to_exclude);

  task_runner->FastForwardBy(
      tab_declutter_controller()->declutter_timer_interval());

  EXPECT_EQ(fake_observer.stale_tabs_processed_count(), 1);

  // Verify that the excluded tab is not part of the processed stale tabs.
  std::vector<tabs::TabModel*> processed_stale_tabs =
      fake_observer.processed_tabs();
  EXPECT_EQ(processed_stale_tabs.size(), 3ul);
  EXPECT_FALSE(std::find(processed_stale_tabs.begin(),
                         processed_stale_tabs.end(),
                         tab_to_exclude) != processed_stale_tabs.end());
}

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestInactiveBrowserDoesNotDeclutterTabs) {
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
      tab_declutter_controller()->declutter_timer_interval());

  EXPECT_EQ(fake_observer.stale_tabs_processed_count(), 0);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 0);
}
