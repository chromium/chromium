// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include <deque>
#include <optional>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace {

// Represents an event reported via `TabListInterfaceObserver`.
struct Event {
  enum class Type {
    TAB_ADDED,
    ACTIVE_TAB_CHANGED,
    TAB_REMOVED,
    TAB_MOVED,
  };

  Event(Type type, raw_ptr<tabs::TabInterface> tab)
      : type(type),
        tab(tab),
        tab_url(tab->GetContents()->GetLastCommittedURL()) {}

  Type type;
  raw_ptr<tabs::TabInterface> tab;

  // The URL of the tab at the time of the event. This is stored separately
  // because `tab` may be null (for removed tabs) or destroyed later.
  GURL tab_url;

  // Used for TAB_MOVED events.
  int from_index = -1;
  int to_index = -1;
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
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override {
    events_.emplace_back(Event::Type::TAB_ADDED, tab);
  }

  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override {
    events_.emplace_back(Event::Type::ACTIVE_TAB_CHANGED, tab);
  }

  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason removed_reason) override {
    Event event(Event::Type::TAB_REMOVED, tab);

    // The tab may be destroyed after removal, so we avoid accessing it later.
    event.tab = nullptr;
    events_.push_back(std::move(event));
  }

  void OnTabMoved(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int from_index,
                  int to_index) override {
    Event event(Event::Type::TAB_MOVED, tab);
    event.from_index = from_index;
    event.to_index = to_index;
    events_.push_back(std::move(event));
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

// Creates `num_tabs` tabs and sets their WebContents IDs to match their
// index with an optional `offset` which is useful if this method is called on
// multiple browser windows within a single test to prevent duplicate IDs.
void SetupTabs(Browser* browser, size_t num_tabs, size_t offset = 0u) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  for (auto i = 0u; i < num_tabs; i++) {
    auto disposition = i == 0u ? WindowOpenDisposition::CURRENT_TAB
                               : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, GURL("about:blank"), disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP))
        << base::StringPrintf("Failed to open tab at index %u.", i);
    SetID(tab_strip_model->GetWebContentsAt(i), i + offset);
  }
}

// TODO(devlin): Would it make sense to port this to instead be a
// TabListInterface browsertest, and use it on all relevant platforms?
class TabListBridgeBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Wait for the TabGroupSyncService to properly initialize before making any
    // changes to tab groups.
    auto observer =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(
                GetProfile()));
    observer->Wait();
  }
};

}  // namespace

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

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, ActivateTab) {
  const GURL url("http://one.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Add a second tab, which should be active (it opens in the foreground).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(2, tab_list_interface->GetTabCount());
  EXPECT_EQ(1, tab_list_interface->GetActiveIndex());

  // Focus the first tab.
  tab_list_interface->ActivateTab(tab_list_interface->GetTab(0)->GetHandle());
  EXPECT_EQ(0, tab_list_interface->GetActiveIndex());

  // (Re)-Focus the second tab.
  tab_list_interface->ActivateTab(tab_list_interface->GetTab(1)->GetHandle());
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

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetOpenerForTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Open three tabs. All should be returned (in order).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Focus the first tab.
  tab_list_interface->ActivateTab(tab_list_interface->GetTab(0)->GetHandle());
  EXPECT_EQ(0, tab_list_interface->GetActiveIndex());

  // Set Opener for the first tab
  tab_list_interface->SetOpenerForTab(
      tab_list_interface->GetTab(0)->GetHandle(),
      tab_list_interface->GetTab(1)->GetHandle());

  // Get Opener for the first tab
  EXPECT_THAT(tab_list_interface->GetOpenerForTab(
                  tab_list_interface->GetTab(0)->GetHandle()),
              MatchesTab(url2));
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

  // Verify that only tab indices 0 and 1 are selected.
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_TRUE(tab_strip_model->IsTabSelected(1));
  EXPECT_FALSE(tab_strip_model->IsTabSelected(2));
  EXPECT_FALSE(tab_strip_model->IsTabSelected(3));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest,
                       ContainsTabGroupWhenTabGroupsNotSupported) {
  // App windows don't allow tab groups.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "some app", /*trusted_source=*/false, gfx::Rect(), browser()->profile(),
      /*user_gesture=*/true);
  // params.window = window2.release();
  Browser* browser2 = Browser::Create(params);
  BrowserList::SetLastActive(browser2);

  ASSERT_FALSE(browser2->tab_strip_model()->SupportsTabGroups());

  TabListInterface* tab_list_interface = TabListInterface::From(browser2);
  ASSERT_TRUE(tab_list_interface);

  // No crash when querying a tab strip that doesn't support groups.
  EXPECT_FALSE(tab_list_interface->ContainsTabGroup(
      tab_groups::TabGroupId::CreateEmpty()));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, AddTabsToGroup) {
  SetupTabs(browser(), 3);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
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

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, SetTabGroupVisualData) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  // Create a group out of the first and only tab.
  tab_groups::TabGroupId group_id = tab_strip_model->AddToNewGroup({0});

  // Add a test observer to the tab strip.
  class TestObserver : public TabStripModelObserver {
   public:
    void OnTabGroupChanged(const TabGroupChange& change) override {
      tab_group_changed_++;
    }

    int tab_group_changed_ = 0;
  } observer;
  tab_strip_model->AddObserver(&observer);

  // Change the visual data for the group via the bridge.
  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  tab_groups::TabGroupVisualData data(u"Title",
                                      tab_groups::TabGroupColorId::kBlue);
  tab_list_interface->SetTabGroupVisualData(group_id, data);

  // The visual data changed.
  std::optional<tab_groups::TabGroupVisualData> new_data =
      tab_list_interface->GetTabGroupVisualData(group_id);
  ASSERT_TRUE(new_data);
  EXPECT_EQ(u"Title", new_data->title());
  EXPECT_EQ(tab_groups::TabGroupColorId::kBlue, new_data->color());

  // The observer fired.
  EXPECT_EQ(1, observer.tab_group_changed_);

  tab_strip_model->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, Ungroup) {
  // Create three tabs.
  SetupTabs(browser(), 3);

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Add the three tabs to a group.
  auto group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(0)->GetHandle(),
                                  tab_list_interface->GetTab(1)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle()});
  EXPECT_TRUE(group_id.has_value());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
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

// Tests moving a tab group to valid indices only.
IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, MoveGroupTo) {
  SetupTabs(browser(), 10);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
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

// Tests moving a tab group to the middle of another group, where the closest
// valid index will be used. The below four tests cover all cases for this.
IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest,
                       MoveGroupTo_MiddleOfGroup_Right) {
  SetupTabs(browser(), 10);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  ASSERT_EQ("0 1 2 3 4 5 6 7 8 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  auto first_group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(0)->GetHandle(),
                                  tab_list_interface->GetTab(1)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle()});
  ASSERT_TRUE(first_group_id.has_value());

  tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(5)->GetHandle(),
                                  tab_list_interface->GetTab(6)->GetHandle(),
                                  tab_list_interface->GetTab(7)->GetHandle(),
                                  tab_list_interface->GetTab(8)->GetHandle(),
                                  tab_list_interface->GetTab(9)->GetHandle()});
  EXPECT_EQ("0g0 1g0 2g0 3 4 5g1 6g1 7g1 8g1 9g1",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Attempt to move the tab group rightwards to index 4, but the closest valid
  // index is 2 so the moved group will end up just to the left of the other
  // group.
  tab_list_interface->MoveGroupTo(*first_group_id, 4);
  EXPECT_EQ("3 4 0g0 1g0 2g0 5g1 6g1 7g1 8g1 9g1",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest,
                       MoveGroupTo_MiddleOfGroup_Right2) {
  SetupTabs(browser(), 10);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  ASSERT_EQ("0 1 2 3 4 5 6 7 8 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  auto first_group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(0)->GetHandle(),
                                  tab_list_interface->GetTab(1)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle()});
  ASSERT_TRUE(first_group_id.has_value());

  tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(5)->GetHandle(),
                                  tab_list_interface->GetTab(6)->GetHandle(),
                                  tab_list_interface->GetTab(7)->GetHandle(),
                                  tab_list_interface->GetTab(8)->GetHandle(),
                                  tab_list_interface->GetTab(9)->GetHandle()});
  EXPECT_EQ("0g0 1g0 2g0 3 4 5g1 6g1 7g1 8g1 9g1",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Attempt to move the tab group rightwards to index 5, but the closest valid
  // index is 7 so the moved group will end up just to the right of the other
  // group.
  tab_list_interface->MoveGroupTo(*first_group_id, 5);
  EXPECT_EQ("3 4 5g0 6g0 7g0 8g0 9g0 0g1 1g1 2g1",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest,
                       MoveGroupTo_MiddleOfGroup_Left) {
  SetupTabs(browser(), 10);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  ASSERT_EQ("0 1 2 3 4 5 6 7 8 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(1)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle(),
                                  tab_list_interface->GetTab(3)->GetHandle()});

  auto group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(6)->GetHandle(),
                                  tab_list_interface->GetTab(7)->GetHandle(),
                                  tab_list_interface->GetTab(8)->GetHandle()});
  ASSERT_TRUE(group_id.has_value());
  EXPECT_EQ("0 1g0 2g0 3g0 4 5 6g1 7g1 8g1 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Attempt to move the tab group leftwards to index 2, but the closest valid
  // index is 1 so the moved group will end up just to the left of the other
  // group.
  tab_list_interface->MoveGroupTo(*group_id, 2);
  EXPECT_EQ("0 6g0 7g0 8g0 1g1 2g1 3g1 4 5 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest,
                       MoveGroupTo_MiddleOfGroup_Left2) {
  SetupTabs(browser(), 10);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  ASSERT_EQ("0 1 2 3 4 5 6 7 8 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(1)->GetHandle(),
                                  tab_list_interface->GetTab(2)->GetHandle(),
                                  tab_list_interface->GetTab(3)->GetHandle()});

  auto group_id = tab_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt, {tab_list_interface->GetTab(6)->GetHandle(),
                                  tab_list_interface->GetTab(7)->GetHandle(),
                                  tab_list_interface->GetTab(8)->GetHandle()});
  ASSERT_TRUE(group_id.has_value());
  EXPECT_EQ("0 1g0 2g0 3g0 4 5 6g1 7g1 8g1 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));

  // Attempt to move the tab group leftwards to index 3, but the closest valid
  // index is 4 so the moved group will end up just to the right of the other
  // group.
  tab_list_interface->MoveGroupTo(*group_id, 3);
  EXPECT_EQ("0 1g0 2g0 3g0 6g1 7g1 8g1 4 5 9",
            GetTabStripStateString(tab_strip_model, /*annotate_groups=*/true));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, OpenTab) {
  const GURL url1("about:blank?q=1");
  const GURL url2("about:blank?q=2");
  const GURL url3("about:blank?q=3");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Open a tab at the start of the tab strip.
  tab_list_interface->OpenTab(url2, 0);
  EXPECT_EQ(0, tab_list_interface->GetActiveIndex());
  EXPECT_TRUE(content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(0)));
  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url2), MatchesTab(url1)));

  // Open a tab at the end of the tab strip by specifying -1 as the index.
  tab_list_interface->OpenTab(url3, -1);
  EXPECT_EQ(2, tab_list_interface->GetActiveIndex());
  EXPECT_TRUE(content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(2)));
  EXPECT_THAT(tab_list_interface->GetAllTabs(),
              testing::ElementsAre(MatchesTab(url2), MatchesTab(url1),
                                   MatchesTab(url3)));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, InsertWebContentsAt) {
  SetupTabs(browser(), 3);

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);
  EXPECT_EQ(3, tab_list_interface->GetTabCount());

  // Insert WebContents to a new tab.
  auto web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          browser()->profile(),
          content::SiteInstance::Create(browser()->profile())));
  auto* web_contents_ptr = web_contents.get();
  tabs::TabInterface* new_tab = tab_list_interface->InsertWebContentsAt(
      2, std::move(web_contents), false, std::nullopt);

  // Now we should have 4 tabs.
  EXPECT_EQ(4, tab_list_interface->GetTabCount());
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab, tab_list_interface->GetTab(2));
  EXPECT_EQ(web_contents_ptr, new_tab->GetContents());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, DiscardTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://one.example"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabListInterface* tab_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list_interface);
  EXPECT_EQ(2, tab_list_interface->GetTabCount());
  EXPECT_EQ(0, tab_list_interface->GetActiveIndex());

  // Check that the second tab is not discarded (yet).
  auto* web_contents = tab_list_interface->GetTab(1)->GetContents();
  EXPECT_NE(mojom::LifecycleUnitState::DISCARDED,
            resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
                web_contents)
                ->GetTabState());

  // Discard the second tab.
  tabs::TabInterface* tab_to_discard = tab_list_interface->GetTab(1);
  auto* discarded_contents =
      tab_list_interface->DiscardTab(tab_to_discard->GetHandle());

  // The second tab should now be discarded.
  EXPECT_EQ(tab_list_interface->GetTab(1)->GetContents(), discarded_contents);
  EXPECT_EQ(mojom::LifecycleUnitState::DISCARDED,
            resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
                discarded_contents)
                ->GetTabState());
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, MoveTabGroupToWindow) {
  // Set up three tabs in two windows, but make sure each tab has its own
  // WebContents ID.

  SetupTabs(browser(), 3);
  TabListInterface* source_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(source_list_interface);
  TabStripModel* source_model = browser()->tab_strip_model();
  ASSERT_TRUE(source_model);

  ASSERT_EQ("0 1 2",
            GetTabStripStateString(source_model, /*annotate_groups=*/true));

  Browser* second_browser = CreateBrowser(browser()->profile());
  SetupTabs(second_browser, 3, /*offset=*/3);
  TabStripModel* destination_model = second_browser->tab_strip_model();
  ASSERT_TRUE(destination_model);

  ASSERT_EQ("3 4 5", GetTabStripStateString(destination_model,
                                            /*annotate_groups=*/true));

  // Group the first two tabs in the source window, then move the group to the
  // second window.
  auto group_id = source_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt,
      {source_list_interface->GetTab(0)->GetHandle(),
       source_list_interface->GetTab(1)->GetHandle()});

  ASSERT_TRUE(group_id.has_value());
  EXPECT_EQ("0g0 1g0 2",
            GetTabStripStateString(source_model, /*annotate_groups=*/true));

  source_list_interface->MoveTabGroupToWindow(*group_id,
                                              second_browser->session_id(), 1);

  // Verify that the group has been moved to the destination window.
  EXPECT_EQ("2",
            GetTabStripStateString(source_model, /*annotate_groups=*/true));
  EXPECT_EQ("3 0g0 1g0 4 5", GetTabStripStateString(destination_model,
                                                    /*annotate_groups=*/true));
}

// Test that moving a group to another window in the middle of another group
// moves it to the closest valid index.
IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest,
                       MoveTabGroupToWindow_MiddleOfGroup) {
  // Set up three tabs in two windows, but make sure each tab has its own
  // WebContents ID.
  SetupTabs(browser(), 3);

  Browser* second_browser = CreateBrowser(browser()->profile());
  SetupTabs(second_browser, 3, /*offset=*/3);

  TabListInterface* source_list_interface = TabListInterface::From(browser());
  ASSERT_TRUE(source_list_interface);

  // Group the first two tabs in the source window.
  auto group_id = source_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt,
      {source_list_interface->GetTab(0)->GetHandle(),
       source_list_interface->GetTab(1)->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  TabStripModel* source_model = browser()->tab_strip_model();
  ASSERT_TRUE(source_model);
  EXPECT_EQ("0g0 1g0 2",
            GetTabStripStateString(source_model, /*annotate_groups=*/true));

  // Group all three tabs in the second window.
  TabListInterface* destination_list_interface =
      TabListInterface::From(second_browser);
  ASSERT_TRUE(destination_list_interface);
  destination_list_interface->AddTabsToGroup(
      /*group_id=*/std::nullopt,
      {destination_list_interface->GetTab(0)->GetHandle(),
       destination_list_interface->GetTab(1)->GetHandle(),
       destination_list_interface->GetTab(2)->GetHandle()});

  // Now move the group to the second window. The group should be moved to the
  // end since the closest valid index that isn't in the middle of another tab
  // group is 3.
  source_list_interface->MoveTabGroupToWindow(*group_id,
                                              second_browser->session_id(), 2);

  EXPECT_EQ("2",
            GetTabStripStateString(source_model, /*annotate_groups=*/true));

  TabStripModel* destination_model = second_browser->tab_strip_model();
  ASSERT_TRUE(destination_model);
  EXPECT_EQ(
      "3g0 4g0 5g0 0g1 1g1",
      GetTabStripStateString(destination_model, /*annotate_groups=*/true));
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, Observer_OnTabRemoved) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Navigate to one.example in the current tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Open a new tab in the background.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  FakeObserver observer(tab_list_interface);

  // Close the second tab.
  tab_list_interface->CloseTab(tab_list_interface->GetTab(1)->GetHandle());

  // We should have received one TAB_CLOSED event corresponding to the second
  // tab.
  EXPECT_EQ(url2, observer.ReadEvent(Event::Type::TAB_REMOVED).tab_url);
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, Observer_OnTabMoved) {
  // Create three tabs.
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");
  const GURL url3("http://three.example");

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  // Navigate to one.example in the current tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Open a new tab in the background.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Open a third tab in the background.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  FakeObserver observer(tab_list_interface);

  // Move the first tab to the end.
  tab_list_interface->MoveTab(tab_list_interface->GetTab(0)->GetHandle(), 2);

  // We should have received one TAB_MOVED event corresponding to the first tab.
  auto event = observer.ReadEvent(Event::Type::TAB_MOVED);
  EXPECT_EQ(url1, event.tab_url);
  EXPECT_EQ(0, event.from_index);
  EXPECT_EQ(2, event.to_index);
}

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, IsTabListEditable) {
  // Use two tab lists, which means two browsers.
  Profile* profile = browser()->profile();
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(profile);

  TabListInterface* tab_list1 = TabListInterface::From(browser1);
  TabListInterface* tab_list2 = TabListInterface::From(browser2);

  // By default, tab lists are editable.
  EXPECT_TRUE(tab_list1->IsThisTabListEditable());
  EXPECT_TRUE(tab_list2->IsThisTabListEditable());
  // And the static check should allow editing.
  EXPECT_TRUE(TabListInterface::CanEditTabList(*profile));

  // Change the first tab list to be un-editable.
  browser1->window()->DisableTabStripEditingForTesting();

  EXPECT_FALSE(tab_list1->IsThisTabListEditable());
  EXPECT_TRUE(tab_list2->IsThisTabListEditable());
  // Since one tab list is ineditable, the global check should not allow
  // editing.
  EXPECT_FALSE(TabListInterface::CanEditTabList(*profile));
}
