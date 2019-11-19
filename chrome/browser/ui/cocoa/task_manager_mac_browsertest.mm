// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include <algorithm>

#include "base/macros.h"
#include "base/strings/pattern.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"

namespace task_manager {

using browsertest_util::WaitForTaskManagerRows;

class TaskManagerMacTest : public InProcessBrowserTest {
 public:
  TaskManagerMacTest() {}
  ~TaskManagerMacTest() override {}

  void SetUpOnMainThread() override {
    ASSERT_FALSE(GetTaskManagerMac());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Make sure the task manager is closed (if any).
    chrome::HideTaskManager();
    ASSERT_FALSE(GetTaskManagerMac());
  }

  TaskManagerMac* GetTaskManagerMac() const {
    return TaskManagerMac::GetInstanceForTests();
  }

  NSTableView* GetTable() const {
    return GetTaskManagerMac() ? [GetTaskManagerMac()->CocoaControllerForTests()
                                         tableViewForTesting]
                               : nullptr;
  }

  int TableFirstSelectedRow() const {
    return [[GetTable() selectedRowIndexes] firstIndex];
  }

  void PressKillButton() {
    [[GetTaskManagerMac()->CocoaControllerForTests() endProcessButtonForTesting]
        performClick:nil];
  }

  void ClearStoredColumnSettings() const {
    PrefService* local_state = g_browser_process->local_state();
    if (!local_state)
      FAIL();

    DictionaryPrefUpdate dict_update(local_state,
                                     prefs::kTaskManagerColumnVisibility);
    dict_update->Clear();
  }

  void ToggleColumnVisibility(TaskManagerMac* task_manager, int col_id) {
    DCHECK(task_manager);
    task_manager->GetTableModelForTests()->ToggleColumnVisibility(col_id);
  }

  // Looks up a tab based on its tab ID.
  content::WebContents* FindWebContentsByTabId(SessionID tab_id) {
    auto& all_tabs = AllTabContentses();
    auto tab_id_matches = [tab_id](content::WebContents* web_contents) {
      return SessionTabHelper::IdForTab(web_contents) == tab_id;
    };
    auto it = std::find_if(all_tabs.begin(), all_tabs.end(), tab_id_matches);

    return (it == all_tabs.end()) ? nullptr : *it;
  }

  // Returns the current TaskManagerTableModel index for a particular tab. Don't
  // cache this value, since it can change whenever the message loop runs.
  int FindRowForTab(content::WebContents* tab) {
    SessionID tab_id = SessionTabHelper::IdForTab(tab);
    std::unique_ptr<TaskManagerTester> tester =
        TaskManagerTester::Create(base::Closure());
    for (int i = 0; i < tester->GetRowCount(); ++i) {
      if (tester->GetTabId(i) == tab_id)
        return i;
    }
    return -1;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerMacTest);
};

// Tests that all defined columns have a corresponding string IDs for keying
// into the user preferences dictionary.
IN_PROC_BROWSER_TEST_F(TaskManagerMacTest, AllColumnsHaveStringIds) {
  for (size_t i = 0; i < kColumnsSize; ++i)
    EXPECT_NE("", GetColumnIdAsString(kColumns[i].id));
}

// In the case of no settings stored in the user preferences local store, test
// that the task manager table starts with the default columns visibility as
// stored in |kColumns|.
IN_PROC_BROWSER_TEST_F(TaskManagerMacTest, TableStartsWithDefaultColumns) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  NSTableView* table = GetTable();
  ASSERT_TRUE(table);

  EXPECT_EQ(0u, [[table sortDescriptors] count]);
  NSArray* tableColumns = [table tableColumns];
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(kColumns[i].id,
              [[[tableColumns objectAtIndex:i] identifier] intValue]);
    EXPECT_EQ(kColumns[i].default_visibility,
              ![[tableColumns objectAtIndex:i] isHidden]);
  }
}

// Tests that changing columns visibility and sort order will be stored upon
// closing the task manager view and restored when re-opened.
IN_PROC_BROWSER_TEST_F(TaskManagerMacTest, ColumnsSettingsAreRestored) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  TaskManagerMac* task_manager = GetTaskManagerMac();
  ASSERT_TRUE(task_manager);
  NSTableView* table = GetTable();
  ASSERT_TRUE(table);

  // Toggle the visibility of all columns.
  EXPECT_EQ(0u, [[table sortDescriptors] count]);
  NSArray* tableColumns = [table tableColumns];
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(kColumns[i].id,
              [[[tableColumns objectAtIndex:i] identifier] intValue]);
    EXPECT_EQ(kColumns[i].default_visibility,
              ![[tableColumns objectAtIndex:i] isHidden]);
    ToggleColumnVisibility(task_manager, kColumns[i].id);
  }

  // Sort by the first visible and initially ascending sortable column. It would
  // be nice to fake a click with -performClick: but that doesn't work (see
  // http://www.cocoabuilder.com/archive/cocoa/177610-programmatically-click-column-header-in-nstableview.html).
  bool is_sorted = false;
  int sorted_col_id = -1;
  for (NSTableColumn* column in tableColumns) {
    if ([column isHidden])
      continue;
    if ([column sortDescriptorPrototype].ascending) {
      // Toggle the sort for a descending sort.
      NSSortDescriptor* newSortDescriptor =
          [[column sortDescriptorPrototype] reversedSortDescriptor];
      [table setSortDescriptors:@[ newSortDescriptor ]];
      is_sorted = true;
      sorted_col_id = [[column identifier] intValue];
      break;
    }
  }

  NSArray* expectedSortDescriptors = [table sortDescriptors];
  EXPECT_EQ(is_sorted, [expectedSortDescriptors count] > 0);

  // Close the task manager view and re-open. Expect the inverse of the default
  // visibility, and the last sort order.
  chrome::HideTaskManager();
  ASSERT_FALSE(GetTaskManagerMac());
  chrome::ShowTaskManager(browser());
  task_manager = GetTaskManagerMac();
  ASSERT_TRUE(task_manager);
  table = GetTable();
  ASSERT_TRUE(table);

  if (is_sorted) {
    EXPECT_NSEQ(expectedSortDescriptors, [table sortDescriptors]);
  }
}

IN_PROC_BROWSER_TEST_F(TaskManagerMacTest, SelectionConsistency) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());

  // Set up a total of three tabs in different processes.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("b.com", "/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("c.com", "/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Wait for their titles to appear in the TaskManager. There should be three
  // rows.
  auto pattern = browsertest_util::MatchTab("Title *");
  int rows = 3;
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(rows, pattern));

  // Find the three tabs we set up, in TaskManager model order. Because we have
  // not sorted the table yet, this should also be their UI display order.
  std::unique_ptr<TaskManagerTester> tester =
      TaskManagerTester::Create(base::Closure());
  std::vector<content::WebContents*> tabs;
  for (int i = 0; i < tester->GetRowCount(); ++i) {
    // Filter based on our title.
    if (!base::MatchPattern(tester->GetRowTitle(i), pattern))
      continue;
    content::WebContents* tab = FindWebContentsByTabId(tester->GetTabId(i));
    EXPECT_NE(nullptr, tab);
    tabs.push_back(tab);
  }
  EXPECT_EQ(3U, tabs.size());

  // Select the middle row, and store its tab id.
  [GetTable()
          selectRowIndexes:[NSIndexSet indexSetWithIndex:FindRowForTab(tabs[1])]
      byExtendingSelection:NO];
  EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1, [GetTable() numberOfSelectedRows]);

  // Add 3 rows above the selection. The selected tab should not change.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(content::ExecuteScript(tabs[0], "window.open('title3.html');"));
    EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[1]));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 3), pattern));
  EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1, [GetTable() numberOfSelectedRows]);

  // Add 2 rows below the selection. The selected tab should not change.
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(content::ExecuteScript(tabs[2], "window.open('title3.html');"));
    EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[1]));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 2), pattern));
  EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1, [GetTable() numberOfSelectedRows]);

  // Add a new row in the same process as the selection. The selected tab should
  // not change.
  ASSERT_TRUE(content::ExecuteScript(tabs[1], "window.open('title3.html');"));
  EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[1]));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 1), pattern));
  EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(2, [GetTable() numberOfSelectedRows]);

  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

    // Press the button, which kills the process of the selected row.
    PressKillButton();

    // Two rows should disappear.
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows -= 2), pattern));
  }

  // No row should now be selected.
  ASSERT_EQ(-1, TableFirstSelectedRow());

  // Now select tabs[2].
  [GetTable()
          selectRowIndexes:[NSIndexSet indexSetWithIndex:FindRowForTab(tabs[2])]
      byExtendingSelection:NO];

  // Focus and reload one of the sad tabs. It should reappear in the TM. The
  // other sad tab should not reappear.
  tabs[1]->GetDelegate()->ActivateContents(tabs[1]);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 1), pattern));

  // tabs[2] should still be selected.
  EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[2]));

  // Close tabs[0]. The selection should not change.
  chrome::CloseWebContents(browser(), tabs[0], false);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows -= 1), pattern));
  EXPECT_EQ(TableFirstSelectedRow(), FindRowForTab(tabs[2]));
}

}  // namespace task_manager
