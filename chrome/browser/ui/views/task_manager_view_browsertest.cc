// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/task_manager_view.h"

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace task_manager {

using browsertest_util::WaitForTaskManagerRows;

class TaskManagerViewTest : public InProcessBrowserTest {
 public:
  TaskManagerViewTest() = default;

  TaskManagerViewTest(const TaskManagerViewTest&) = delete;
  TaskManagerViewTest& operator=(const TaskManagerViewTest&) = delete;

  ~TaskManagerViewTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Make sure the task manager is closed (if any).
    chrome::HideTaskManager();
    content::RunAllPendingInMessageLoop();
    ASSERT_FALSE(GetView());
  }

  TaskManagerView* GetView() const {
    return TaskManagerView::GetInstanceForTests();
  }

  views::TableView* GetTable() const {
    return GetView() ? GetView()->tab_table_.get() : nullptr;
  }

  void PressKillButton() { GetView()->Accept(); }

  void ClearStoredColumnSettings() const {
    PrefService* local_state = g_browser_process->local_state();
    if (!local_state)
      FAIL();

    ScopedDictPrefUpdate dict_update(local_state,
                                     prefs::kTaskManagerColumnVisibility);
    dict_update->clear();
  }

  void ToggleColumnVisibility(TaskManagerView* view, int col_id) {
    DCHECK(view);
    view->table_model_->ToggleColumnVisibility(col_id);
  }

  // Looks up a tab based on its tab ID.
  content::WebContents* FindWebContentsByTabId(SessionID tab_id) {
    auto& all_tabs = AllTabContentses();
    auto it = base::ranges::find(all_tabs, tab_id,
                                 &sessions::SessionTabHelper::IdForTab);
    return (it == all_tabs.end()) ? nullptr : *it;
  }

  // Returns the current TaskManagerTableModel index for a particular tab. Don't
  // cache this value, since it can change whenever the message loop runs.
  std::optional<size_t> FindRowForTab(content::WebContents* tab) {
    SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
    std::unique_ptr<TaskManagerTester> tester =
        TaskManagerTester::Create(base::RepeatingClosure());
    for (size_t i = 0; i < tester->GetRowCount(); ++i) {
      if (tester->GetTabId(i) == tab_id)
        return i;
    }
    return std::nullopt;
  }

  void HideTaskManagerSync() {
    views::test::WidgetDestroyedWaiter waiter(GetView()->GetWidget());
    chrome::HideTaskManager();
    waiter.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that all defined columns have a corresponding string IDs for keying
// into the user preferences dictionary.
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, AllColumnsHaveStringIds) {
  for (size_t i = 0; i < kColumnsSize; ++i)
    EXPECT_NE("", GetColumnIdAsString(kColumns[i].id));
}

// Test that all defined columns can be sorted
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, AllColumnsHaveSortable) {
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_TRUE(kColumns[i].sortable);
  }
}

// In the case of no settings stored in the user preferences local store, test
// that the task manager table starts with the default columns visibility as
// stored in |kColumns|.
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, TableStartsWithDefaultColumns) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  views::TableView* table = GetTable();
  ASSERT_TRUE(table);

  EXPECT_FALSE(table->GetIsSorted());
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(kColumns[i].default_visibility,
              table->IsColumnVisible(kColumns[i].id));
  }
}

// Tests that changing columns visibility and sort order will be stored upon
// closing the task manager view and restored when re-opened.
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, ColumnsSettingsAreRestored) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  TaskManagerView* view = GetView();
  ASSERT_TRUE(view);
  views::TableView* table = GetTable();
  ASSERT_TRUE(table);

  // Toggle the visibility of all columns.
  EXPECT_FALSE(table->GetIsSorted());
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(kColumns[i].default_visibility,
              table->IsColumnVisible(kColumns[i].id));
    ToggleColumnVisibility(view, kColumns[i].id);
  }

  // Sort by the first visible and initially ascending sortable column.
  bool is_sorted = false;
  int sorted_col_id = -1;
  for (size_t i = 0; i < table->visible_columns().size(); ++i) {
    const ui::TableColumn& column = table->visible_columns()[i].column;
    if (column.sortable && column.initial_sort_is_ascending) {
      // Toggle the sort twice for a descending sort.
      table->ToggleSortOrder(i);
      table->ToggleSortOrder(i);
      is_sorted = true;
      sorted_col_id = column.id;
      break;
    }
  }

  if (is_sorted) {
    EXPECT_TRUE(table->GetIsSorted());
    EXPECT_FALSE(table->sort_descriptors().front().ascending);
    EXPECT_EQ(table->sort_descriptors().front().column_id, sorted_col_id);
  }

  // Close the task manager view and re-open. Expect the inverse of the default
  // visibility, and the last sort order.
  chrome::HideTaskManager();
  content::RunAllPendingInMessageLoop();
  ASSERT_FALSE(GetView());
  chrome::ShowTaskManager(browser());
  view = GetView();
  ASSERT_TRUE(view);
  table = GetTable();
  ASSERT_TRUE(table);

  if (is_sorted) {
    EXPECT_TRUE(table->GetIsSorted());
    EXPECT_FALSE(table->sort_descriptors().front().ascending);
    EXPECT_EQ(table->sort_descriptors().front().column_id, sorted_col_id);
  }
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(!kColumns[i].default_visibility,
              table->IsColumnVisible(kColumns[i].id));
  }
}

// Test hiding all visible columns and keeping them normal when reopened
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, HideAllColumnsAndRestored) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  TaskManagerView* view = GetView();
  ASSERT_TRUE(view);
  views::TableView* table = GetTable();
  ASSERT_TRUE(table);

  EXPECT_FALSE(table->GetIsSorted());

  // hide all visible columns except IDS_TASK_MANAGER_TASK_COLUMN
  int task_column_index = -1;
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(kColumns[i].default_visibility,
              table->IsColumnVisible(kColumns[i].id));
    if (kColumns[i].id == IDS_TASK_MANAGER_TASK_COLUMN) {
      task_column_index = i;
      ASSERT_EQ(kColumns[i].default_visibility,
                table->IsColumnVisible(kColumns[i].id));
      continue;
    }
    if (kColumns[i].default_visibility) {
      ToggleColumnVisibility(view, kColumns[i].id);
    }
  }
  EXPECT_EQ(table->visible_columns().size(), 1u);
  // hide IDS_TASK_MANAGER_TASK_COLUMN
  ASSERT_EQ(task_column_index, 0);
  ToggleColumnVisibility(view, kColumns[task_column_index].id);
  EXPECT_EQ(table->visible_columns().size(), 1u);
  EXPECT_TRUE(table->IsColumnVisible(kColumns[task_column_index].id));

  // Close the task manager view and re-open. Expect
  // IDS_TASK_MANAGER_TASK_COLUMN visibility
  HideTaskManagerSync();
  ASSERT_FALSE(GetView());
  chrome::ShowTaskManager(browser());
  table = GetTable();
  ASSERT_TRUE(table);

  EXPECT_EQ(table->visible_columns().size(), 1u);
  EXPECT_TRUE(table->IsColumnVisible(kColumns[task_column_index].id));
}

IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, InitialSelection) {
  // Navigate the first tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("b.com", "/title3.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // When the task manager is initially shown, the row for the active tab should
  // be selected.
  chrome::ShowTaskManager(browser());

  EXPECT_EQ(1UL, GetTable()->selection_model().size());
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(),
            FindRowForTab(browser()->tab_strip_model()->GetWebContentsAt(1)));

  // Activate tab 0. The selection should not change.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(),
            FindRowForTab(browser()->tab_strip_model()->GetWebContentsAt(1)));

  // If the user re-triggers chrome::ShowTaskManager (e.g. via shift-esc), this
  // should set the TaskManager selection to the active tab.
  chrome::ShowTaskManager(browser());

  EXPECT_EQ(1UL, GetTable()->selection_model().size());
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(),
            FindRowForTab(browser()->tab_strip_model()->GetWebContentsAt(0)));
}

// Test is flaky. https://crbug.com/998403
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, DISABLED_SelectionConsistency) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());

  // Set up a total of three tabs in different processes.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("b.com", "/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("c.com", "/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Wait for their titles to appear in the TaskManager. There should be three
  // rows.
  auto pattern = browsertest_util::MatchTab("Title *");
  int rows = 3;
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(rows, pattern));

  // Find the three tabs we set up, in TaskManager model order. Because we have
  // not sorted the table yet, this should also be their UI display order.
  std::unique_ptr<TaskManagerTester> tester =
      TaskManagerTester::Create(base::RepeatingClosure());
  std::vector<content::WebContents*> tabs;
  for (size_t i = 0; i < tester->GetRowCount(); ++i) {
    // Filter based on our title.
    if (!base::MatchPattern(tester->GetRowTitle(i), pattern))
      continue;
    content::WebContents* tab = FindWebContentsByTabId(tester->GetTabId(i));
    EXPECT_NE(nullptr, tab);
    tabs.push_back(tab);
  }
  EXPECT_EQ(3U, tabs.size());

  // Select the middle row, and store its tab id.
  GetTable()->Select(FindRowForTab(tabs[1]).value());
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  // Add 3 rows above the selection. The selected tab should not change.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(content::ExecJs(tabs[0], "window.open('title3.html');"));
    EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[1]));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 3), pattern));
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  // Add 2 rows below the selection. The selected tab should not change.
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(content::ExecJs(tabs[2], "window.open('title3.html');"));
    EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[1]));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 2), pattern));
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  // Add a new row in the same process as the selection. The selected tab should
  // not change.
  ASSERT_TRUE(content::ExecJs(tabs[1], "window.open('title3.html');"));
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[1]));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 1), pattern));
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

    // Press the button, which kills the process of the selected row.
    PressKillButton();

    // Two rows should disappear.
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows -= 2), pattern));
  }

  // A later row should now be selected. The selection should be after the 4
  // rows sharing the tabs[0] process, and it should be at or before
  // the tabs[2] row.
  ASSERT_LT(FindRowForTab(tabs[0]).value() + 3,
            GetTable()->GetFirstSelectedRow());
  ASSERT_LE(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[2]));

  // Now select tabs[2].
  GetTable()->Select(FindRowForTab(tabs[2]).value());

  // Focus and reload one of the sad tabs. It should reappear in the TM. The
  // other sad tab should not reappear.
  tabs[1]->GetDelegate()->ActivateContents(tabs[1]);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 1), pattern));

  // tabs[2] should still be selected.
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[2]));

  // Close tabs[0]. The selection should not change.
  chrome::CloseWebContents(browser(), tabs[0], false);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows -= 1), pattern));
  EXPECT_EQ(GetTable()->GetFirstSelectedRow(), FindRowForTab(tabs[2]));
}

// Make sure the task manager's bounds are saved across instances on Chrome OS.
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, RestoreBounds) {
  chrome::ShowTaskManager(browser());

  const gfx::Rect default_bounds =
      GetView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect non_default_bounds = default_bounds + gfx::Vector2d(0, 17);

  GetView()->GetWidget()->SetBounds(non_default_bounds);
  HideTaskManagerSync();

  chrome::ShowTaskManager(browser());
  EXPECT_EQ(non_default_bounds,
            GetView()->GetWidget()->GetWindowBoundsInScreen());

  // Also make sure that the task manager is not restored off-screen.
  // This is a regression test for https://crbug.com/308606
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(non_default_bounds);
  const gfx::Rect offscreen_bounds =
      default_bounds + gfx::Vector2d(0, display.bounds().bottom());
  GetView()->GetWidget()->SetBounds(offscreen_bounds);
  HideTaskManagerSync();

  chrome::ShowTaskManager(browser());
  gfx::Rect restored_bounds = GetView()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_NE(offscreen_bounds, restored_bounds);
  EXPECT_TRUE(display.bounds().Contains(restored_bounds));
}

IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, CloseByAccelerator) {
  chrome::ShowTaskManager(browser());

  EXPECT_FALSE(GetView()->GetWidget()->IsClosed());

  GetView()->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));

  EXPECT_TRUE(GetView()->GetWidget()->IsClosed());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, AppType) {
  chrome::ShowTaskManager(browser());

  EXPECT_EQ(chromeos::AppType::SYSTEM_APP,
            GetView()->GetWidget()->GetNativeWindow()->GetProperty(
                chromeos::kAppTypeKey));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace task_manager
