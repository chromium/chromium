// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include <deque>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

// TODO(devlin): Would it make sense to port this to instead be a
// TabListInterface browsertest, and use it on all relevant platforms?
using TabListBridgeBrowserTest = InProcessBrowserTest;

// Represents an event reported via `TabListInterfaceObserver`.
struct Event {
  enum class Type {
    TAB_ADDED,
    ACTIVE_TAB_CHANGED,
  };
  Type type;
  raw_ptr<tabs::TabInterface> tab;
};

// A fake implementation of TabListInterfaceObserver that records callback
// invocations as `Event`s.
class FakeObserver : public TabListInterfaceObserver {
 public:
  explicit FakeObserver(TabListInterface* tab_list) {
    observation_.Observe(tab_list);
  }

  // Reads an event of the specified type, discarding events of other types
  // reported before the event.
  Event ReadEvent(Event::Type type) {
    while (true) {
      CHECK(!events_.empty()) << "No event of the specified type reported";
      Event event = std::move(events_.front());
      events_.pop_front();
      if (event.type == type) {
        return event;
      }
    }
  }

  // TabListInterfaceObserver:
  void OnTabAdded(tabs::TabInterface* tab, int index) override {
    events_.push_back(Event{Event::Type::TAB_ADDED, tab});
  }

  void OnActiveTabChanged(tabs::TabInterface* tab) override {
    events_.push_back(Event{Event::Type::ACTIVE_TAB_CHANGED, tab});
  }

 private:
  std::deque<Event> events_;
  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      observation_{this};
};

// A helpful matcher for tabs having an expected URL. Since we assume the
// TabInterface works, this is sufficient to meaningfully describe tabs in
// expectations.
MATCHER_P(MatchesTab, expected_url, "") {
  const GURL& actual_url = arg->GetContents()->GetLastCommittedURL();
  bool match = testing::ExplainMatchResult(testing::Eq(expected_url),
                                           actual_url, result_listener);
  if (!match) {
    *result_listener << " Actual URL: " << actual_url;
  }
  return match;
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tabs::TabInterface* tab1 = tab_list_interface->GetTab(0);
  ASSERT_TRUE(tab1);
  EXPECT_THAT(tab1, MatchesTab(url1));

  tabs::TabInterface* tab2 = tab_list_interface->GetTab(1);
  ASSERT_TRUE(tab2);
  EXPECT_THAT(tab2, MatchesTab(url2));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetActiveIndex) {
  const GURL url("http://one.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  EXPECT_EQ(0, tab_list_interface->GetActiveIndex());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(1, tab_list_interface->GetActiveIndex());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetTabCount) {
  const GURL url("http://one.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  EXPECT_EQ(1, tab_list_interface->GetTabCount());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(2, tab_list_interface->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetAllTabs) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Navigate to one.example. This should be the only tab, initially.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url1)));

  // Open two more tabs, for a total of three. All should be returned (in
  // order).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url1), MatchesTab(url2),
                                   MatchesTab(url3)));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetActiveTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Navigate to one.example. This should be the only tab, initially.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetActiveTab(), MatchesTab(url1));

  // Open a new tab in the background. The active tab should be unchanged.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetActiveTab(), MatchesTab(url1));

  // Open a new tab in the foreground. Now, the active tab should be the new
  // tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_THAT(tab_list_interface->GetActiveTab(), MatchesTab(url3));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, PinAndUnpin) {
  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tabs::TabInterface* tab = tab_list_interface->GetActiveTab();
  ASSERT_TRUE(tab);

  EXPECT_FALSE(tab->IsPinned());

  tab_list_interface->PinTab(tab->GetHandle());
  EXPECT_TRUE(tab->IsPinned());

  tab_list_interface->UnpinTab(tab->GetHandle());
  EXPECT_FALSE(tab->IsPinned());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetIndexOfTab) {
  const GURL url("http://example.com");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tabs::TabInterface* tab0 = tab_list_interface->GetActiveTab();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab1 = tab_list_interface->GetActiveTab();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab2 = tab_list_interface->GetActiveTab();

  EXPECT_EQ(0, tab_list_interface->GetIndexOfTab(tab0->GetHandle()));
  EXPECT_EQ(1, tab_list_interface->GetIndexOfTab(tab1->GetHandle()));
  EXPECT_EQ(2, tab_list_interface->GetIndexOfTab(tab2->GetHandle()));

  Browser* new_browser = CreateBrowser(browser()->profile());
  TabListInterface* new_tab_list_interface = TabListBridge::From(new_browser);
  ASSERT_TRUE(new_tab_list_interface);

  tabs::TabInterface* new_tab = new_tab_list_interface->GetActiveTab();

  EXPECT_EQ(-1, tab_list_interface->GetIndexOfTab(new_tab->GetHandle()));
  EXPECT_EQ(-1, new_tab_list_interface->GetIndexOfTab(tab0->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, DuplicateTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");

  // Open two tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  EXPECT_EQ(2, tab_list_interface->GetTabCount());

  // Duplicate the first tab.
  tabs::TabInterface* tab_to_duplicate = tab_list_interface->GetTab(0);
  tab_list_interface->DuplicateTab(tab_to_duplicate->GetHandle());

  // There should now be three tabs, with the duplicated tab inserted next to
  // the original.
  EXPECT_EQ(3, tab_list_interface->GetTabCount());
  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url1), MatchesTab(url1),
                                   MatchesTab(url2)));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, CloseTab) {
  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  EXPECT_EQ(1, tab_list_interface->GetTabCount());

  const GURL url("http://one.example");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(2, tab_list_interface->GetTabCount());

  tabs::TabInterface* tab_to_close = tab_list_interface->GetActiveTab();
  ASSERT_TRUE(tab_to_close);

  tab_list_interface->CloseTab(tab_to_close->GetHandle());
  EXPECT_EQ(1, tab_list_interface->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, MoveTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  // Open three tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url1), MatchesTab(url2),
                                   MatchesTab(url3)));

  // Move the first tab to the end.
  tabs::TabInterface* tab_to_move = tab_list_interface->GetTab(0);
  tab_list_interface->MoveTab(tab_to_move->GetHandle(), 2);
  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url2), MatchesTab(url3),
                                   MatchesTab(url1)));

  // Move the new first tab (originally second) to the middle.
  tab_to_move = tab_list_interface->GetTab(0);
  tab_list_interface->MoveTab(tab_to_move->GetHandle(), 1);
  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url3), MatchesTab(url2),
                                   MatchesTab(url1)));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, MoveTabToWindow) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");

  // Open two tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabListInterface* source_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(source_list_interface);

  // Create a second browser.
  Browser* second_browser = CreateBrowser(browser()->profile());
  TabListInterface* destination_list_interface =
      TabListInterface::From(second_browser);
  ASSERT_TRUE(destination_list_interface);

  EXPECT_EQ(2, source_list_interface->GetTabCount());
  EXPECT_EQ(1, destination_list_interface->GetTabCount());

  // Move the second tab from the first browser to the second.
  tabs::TabInterface* tab_to_move = source_list_interface->GetTab(1);
  source_list_interface->MoveTabToWindow(tab_to_move->GetHandle(),
                                         second_browser->session_id(), 1);

  // Verify the tabs are in the correct places.
  EXPECT_EQ(1, source_list_interface->GetTabCount());
  EXPECT_EQ(2, destination_list_interface->GetTabCount());

  EXPECT_THAT(source_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url1)));
  EXPECT_THAT(destination_list_interface->GetAllTabs(),
              testing::ElementsAre(testing::_, MatchesTab(url2)));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, Observer_OnTabAdded) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  FakeObserver observer(tab_list_interface);

  // Navigate to one.example in the current tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Open a new tab in the background.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Open a new tab in the foreground.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // We should have received two TAB_ADDED events.
  EXPECT_THAT(observer.ReadEvent(Event::Type::TAB_ADDED).tab, MatchesTab(url2));
  EXPECT_THAT(observer.ReadEvent(Event::Type::TAB_ADDED).tab, MatchesTab(url3));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, Observer_OnActiveTabChanged) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  FakeObserver observer(tab_list_interface);

  // Navigate to one.example in the current tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Open a new tab in the background.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Open a new tab in the foreground.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // We should have received single ACTIVE_TAB_CHANGED event.
  EXPECT_THAT(observer.ReadEvent(Event::Type::ACTIVE_TAB_CHANGED).tab,
              MatchesTab(url3));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, HighlightTabs) {
  // Create four tabs: initially the tab with `url4` is active.
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");
  const GURL url4("http://four.example");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url4, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  EXPECT_EQ(3, tab_strip_model->active_index());

  // Initially, only the active tab is selected.
  EXPECT_FALSE(tab_strip_model->IsTabSelected(0));
  EXPECT_FALSE(tab_strip_model->IsTabSelected(1));
  EXPECT_FALSE(tab_strip_model->IsTabSelected(2));
  EXPECT_TRUE(tab_strip_model->IsTabSelected(3));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Select the tabs with `url1` and `url2` and make the tab with `url2` the
  // active tab.
  std::set<tabs::TabHandle> tabs_to_select;
  tabs_to_select.insert(tab_list_interface->GetTab(0)->GetHandle());
  tabs_to_select.insert(tab_list_interface->GetTab(1)->GetHandle());

  tab_list_interface->HighlightTabs(tab_list_interface->GetTab(1)->GetHandle(),
                                    tabs_to_select);

  EXPECT_EQ(1, tab_strip_model->active_index());

  // Verify that the tab with `url4` is still selected since it was the previous
  // active tab (which is selected), but the tab with `url3` is not selected.
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_TRUE(tab_strip_model->IsTabSelected(1));
  EXPECT_FALSE(tab_strip_model->IsTabSelected(2));
  EXPECT_TRUE(tab_strip_model->IsTabSelected(3));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, AddTabsToGroup) {
  // Create three tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  // Set the WebContents ID for all three tabs to their respective indices.
  SetID(tab_strip_model->GetWebContentsAt(0), 0);
  SetID(tab_strip_model->GetWebContentsAt(1), 1);
  SetID(tab_strip_model->GetWebContentsAt(2), 2);

  EXPECT_EQ("0 1 2",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  auto group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(0)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle()});
  ASSERT_TRUE(group_id.has_value());
  EXPECT_EQ("0g0 2g0 1",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Note: The tab with WebContents ID of 1 is now at index 2 after the
  // `AddTabsToGroup` call above.
  auto second_call_group_id = tab_list_interface->AddTabsToGroup(
      group_id, {tab_list_interface->GetTab(2)->GetHandle()});
  EXPECT_EQ(group_id, second_call_group_id);
  EXPECT_EQ("0g0 2g0 1g0",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
}

// Test that calling AddTabsToGroup with a nonexistent ID is a no-op.
IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest,
                       AddTabsToGroup_NonexistentGroupID) {
  // This test only needs one tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  // Set the WebContents ID for the tab to its index.
  SetID(tab_strip_model->GetWebContentsAt(0), 0);
  EXPECT_EQ("0",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Add the tab to a group then ungroup it. This is done to save the `group_id`
  // which then points to a nonexistent tab group after the ungroup call.
  auto group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(0)->GetHandle()});
  ASSERT_TRUE(group_id.has_value());
  EXPECT_EQ("0g0",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  tab_list_interface->Ungroup({tab_list_interface->GetTab(0)->GetHandle()});
  EXPECT_EQ("0",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Trying to add the tab to the now invalid `group_id` should be a no-op.
  auto second_call_group_id = tab_list_interface->AddTabsToGroup(
      group_id, {tab_list_interface->GetTab(0)->GetHandle()});
  EXPECT_FALSE(second_call_group_id.has_value());
  EXPECT_EQ("0",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, Ungroup) {
  // Create three tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  // Set the WebContents ID for all three tabs to their respective indices.
  SetID(tab_strip_model->GetWebContentsAt(0), 0);
  SetID(tab_strip_model->GetWebContentsAt(1), 1);
  SetID(tab_strip_model->GetWebContentsAt(2), 2);

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Add the three tabs to a group.
  auto group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(0)->GetHandle(),
                                  tab_list_interface->GetTab(1)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle()});
  EXPECT_TRUE(group_id.has_value());
  EXPECT_EQ("0g0 1g0 2g0",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Ungroup the tab with WebContents ID 1. Note that the tab shifts to the
  // right so the group can remain contiguous.
  tab_list_interface->Ungroup({tab_list_interface->GetTab(1)->GetHandle()});
  EXPECT_EQ("0g0 2g0 1",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Ungroup the remaining two tabs.
  tab_list_interface->Ungroup({tab_list_interface->GetTab(1)->GetHandle(),
                               tab_list_interface->GetTab(0)->GetHandle()});
  EXPECT_EQ("0 2 1",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, MoveGroupTo) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  // Creates `num_tabs` tabs and sets their WebContents IDs to match their
  // index.
  auto setup_tabs = [this, tab_strip_model](size_t num_tabs) {
    for (auto i = 0u; i < num_tabs; i++) {
      auto disposition = i == 0u ? WindowOpenDisposition::CURRENT_TAB
                                 : WindowOpenDisposition::NEW_BACKGROUND_TAB;
      ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
          browser(), GURL("about:blank"), disposition,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP))
          << base::StringPrintf("Failed to open tab at index %u.", i);
      SetID(tab_strip_model->GetWebContentsAt(i), i);
    }
  };

  setup_tabs(10);
  ASSERT_EQ("0 1 2 3 4 5 6 7 8 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  auto first_group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(0)->GetHandle(),
                                  tab_list_interface->GetTab(1)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle()});
  ASSERT_TRUE(first_group_id.has_value());

  EXPECT_EQ("0g0 1g0 2g0 3 4 5 6 7 8 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Move the tab group to the right by 1.
  tab_list_interface->MoveGroupTo(*first_group_id, 1);
  EXPECT_EQ("3 0g0 1g0 2g0 4 5 6 7 8 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Group the tabs with WebContents IDs [6 7] and [8 9], then move
  // `first_group_id` between them.
  auto second_group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(8)->GetHandle(),
                                  tab_list_interface->GetTab(9)->GetHandle()});
  auto third_group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(6)->GetHandle(),
                                  tab_list_interface->GetTab(7)->GetHandle()});
  EXPECT_EQ("3 0g0 1g0 2g0 4 5 6g1 7g1 8g2 9g2",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  tab_list_interface->MoveGroupTo(*first_group_id, 5);

  // Note that `GetTabStripStateString` group annotation is meant to identify
  // which tabs are in the same group, not the order of group IDs, hence the
  // group ID checks below it.
  EXPECT_EQ("3 4 5 6g0 7g0 0g1 1g1 2g1 8g2 9g2",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
  EXPECT_EQ(third_group_id, tab_strip_model->GetTabGroupForTab(3));
  EXPECT_EQ(first_group_id, tab_strip_model->GetTabGroupForTab(5));
  EXPECT_EQ(second_group_id, tab_strip_model->GetTabGroupForTab(8));
}
