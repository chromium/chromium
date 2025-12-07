// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/breadcrumb_manager_browser_agent.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const base::circular_deque<std::string>& GetEvents() {
  return breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
}

}  // namespace

// Test fixture for testing BreadcrumbManagerBrowserAgent class.
class BreadcrumbManagerBrowserAgentTest : public InProcessBrowserTest {
 protected:
  void AddNewTab(Browser* browser) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

 private:
  breadcrumbs::ScopedEnableBreadcrumbsForTesting scoped_enable_breadcrumbs_;
};

// Tests that adding a new tab generates breadcrumb events.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerBrowserAgentTest,
                       AddNewTabGeneratesEvents) {
  const auto count_tab_events = [&]() {
    const auto& events = GetEvents();
    return std::ranges::count_if(events, [](const auto& event) {
      return event.find("Tab") != std::string::npos;
    });
  };
  const size_t initial_tab_events_count = count_tab_events();
  AddNewTab(browser());
  EXPECT_GT(count_tab_events(), initial_tab_events_count);
}

// Tests that tab insertion events from different browser instances are
// distinguishable in the breadcrumb manager.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerBrowserAgentTest,
                       MultipleBrowsersGenerateDistinctInsertEvents) {
  // Create a second browser instance. Each browser starts with a tab that is
  // inserted into its tab strip, so we should get exactly two "Insert" events.
  Browser* browser2 = CreateBrowser(GetProfile());
  EXPECT_TRUE(browser2);

  const auto& events = GetEvents();

  std::vector<std::string> insert_tab_events;
  std::ranges::copy_if(events, std::back_inserter(insert_tab_events),
                       [](const auto& event) {
                         return event.find("Insert") != std::string::npos;
                       });

  EXPECT_EQ(insert_tab_events.size(), 2u);

  const std::string event1 = insert_tab_events[0];
  const std::string event2 = insert_tab_events[1];

  // Compare the end of the event strings to ensure uniqueness at the tab
  // layer. The tabs are different and that needs to be represented in the
  // event string.
  const size_t event1_split_pos = event1.find("Insert");
  const size_t event2_split_pos = event2.find("Insert");

  const std::string event1_tab_part =
      event1.substr(event1_split_pos, event1.length() - event1_split_pos);
  const std::string event2_tab_part =
      event2.substr(event2_split_pos, event2.length() - event2_split_pos);
  EXPECT_NE(event1_tab_part, event2_tab_part);

  CloseBrowserSynchronously(browser2);
}

// Tests that batch tab closing operations generate appropriate breadcrumb
// events.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerBrowserAgentTest,
                       BatchTabClosingGeneratesEvents) {
  int initial_tab_count = browser()->tab_strip_model()->count();

  AddNewTab(browser());
  AddNewTab(browser());

  EXPECT_GT(browser()->tab_strip_model()->count(), initial_tab_count);

  // Perform batch tab closing.
  browser()->tab_strip_model()->CloseAllTabs();

  const auto& events = GetEvents();
  bool found_close_event = std::ranges::any_of(events, [](const auto& event) {
    return event.find("Closed") != std::string::npos &&
           event.find("tabs") != std::string::npos;
  });
  EXPECT_TRUE(found_close_event);
}
