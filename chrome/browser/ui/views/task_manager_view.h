// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/table_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
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
  METADATA_HEADER(TaskManagerView, views::DialogDelegateView)

 public:
  TaskManagerView(const TaskManagerView&) = delete;
  TaskManagerView& operator=(const TaskManagerView&) = delete;
  ~TaskManagerView() override;

  // Shows the Task Manager window, or re-activates an existing one.
  static task_manager::TaskManagerTableModel* Show(Browser* browser);

  // Hides the Task Manager if it is showing.
  static void Hide();

  // task_manager::TableViewDelegate:
  bool IsColumnVisible(int column_id) const override;
  bool SetColumnVisibility(int column_id, bool new_visibility) override;
  bool IsTableSorted() const override;
  TableSortDescriptor GetSortDescriptor() const override;
  void SetSortDescriptor(const TableSortDescriptor& descriptor) override;
  void MaybeHighlightActiveTask() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // views::DialogDelegateView:
  views::View* GetInitiallyFocusedView() override;
  bool ExecuteWindowsCommand(int command_id) override;
  ui::ImageModel GetWindowIcon() override;
  std::string GetWindowName() const override;
  bool Accept() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  void WindowClosing() override;

  // views::TableGrouper:
  void GetGroupRange(size_t model_index, views::GroupRange* range) override;

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

  views::TableView* tab_table_for_testing() { return tab_table_; }

  static TaskManagerView* GetInstanceForTests();

 private:
  friend class TaskManagerViewTest;

  TaskManagerView();

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
  std::u16string always_on_top_menu_text_;

  raw_ptr<views::TableView, DanglingUntriaged> tab_table_;
  raw_ptr<views::View, DanglingUntriaged> tab_table_parent_;

  // all possible columns, not necessarily visible
  std::vector<ui::TableColumn> columns_;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_
