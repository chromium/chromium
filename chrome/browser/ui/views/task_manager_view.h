// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/table_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace views {
class TableView;
class View;
}  // namespace views

namespace task_manager {

// The new task manager UI container.
class TaskManagerView : public TableViewDelegate,
                        public views::DialogDelegateView,
                        public views::TableGrouper,
                        public views::TableViewObserver,
                        public views::ContextMenuController,
                        public ui::SimpleMenuModel::Delegate {
 public:
  ~TaskManagerView() override;

  // Shows the Task Manager window, or re-activates an existing one.
  static task_manager::TaskManagerTableModel* Show(Browser* browser);

  // Hides the Task Manager if it is showing.
  static void Hide();

  // task_manager::TableViewDelegate:
  bool IsColumnVisible(int column_id) const override;
  void SetColumnVisibility(int column_id, bool new_visibility) override;
  bool IsTableSorted() const override;
  TableSortDescriptor GetSortDescriptor() const override;
  void SetSortDescriptor(const TableSortDescriptor& descriptor) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // views::DialogDelegateView:
  views::View* GetInitiallyFocusedView() override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool ExecuteWindowsCommand(int command_id) override;
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  std::string GetWindowName() const override;
  bool Accept() override;
  bool Close() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  void WindowClosing() override;

  // views::TableGrouper:
  void GetGroupRange(int model_index, views::GroupRange* range) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnKeyDown(ui::KeyboardCode keycode) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override;
  bool IsCommandIdEnabled(int id) const override;
  void ExecuteCommand(int id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

 private:
  friend class TaskManagerViewTest;

  TaskManagerView();

  static TaskManagerView* GetInstanceForTests();

  // Creates the child controls.
  void Init();

  // Initializes the state of the always-on-top setting as the window is shown.
  void InitAlwaysOnTopState();

  // Activates the tab associated with the selected row.
  void ActivateSelectedTab();

  // Selects the active tab in the specified browser window.
  void SelectTaskOfActiveTab(Browser* browser);

  // Restores saved "always on top" state from a previous session.
  void RetrieveSavedAlwaysOnTopState();

  std::unique_ptr<TaskManagerTableModel> table_model_;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // We need to own the text of the menu, the Windows API does not copy it.
  base::string16 always_on_top_menu_text_;

  views::TableView* tab_table_;
  views::View* tab_table_parent_;

  // all possible columns, not necessarily visible
  std::vector<ui::TableColumn> columns_;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerView);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_
