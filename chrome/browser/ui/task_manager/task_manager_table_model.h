// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "ui/base/models/table_model.h"

namespace content {
class WebContents;
}

namespace task_manager {

class TaskManagerValuesStringifier;

// Describes how the platform specific table view is sorted.
struct TableSortDescriptor {
  TableSortDescriptor();
  TableSortDescriptor(int col_id, bool ascending);

  // The ID of the sorted column, -1 if the table is not sorted.
  int sorted_column_id;

  // True if the column is sorted ascending.
  bool is_ascending;
};

// An interface to be able to communicate with the platform-specific table view
// (Either views::TableView or NSTableView on the Mac).
class TableViewDelegate {
 public:
  TableViewDelegate() {}
  virtual ~TableViewDelegate() {}

  virtual bool IsColumnVisible(int column_id) const = 0;

  virtual void SetColumnVisibility(int column_id, bool new_visibility) = 0;

  virtual bool IsTableSorted() const = 0;

  virtual TableSortDescriptor GetSortDescriptor() const = 0;

  virtual void SetSortDescriptor(
      const TableSortDescriptor& sort_descriptor) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TableViewDelegate);
};

class TaskManagerTableModel
    : public TaskManagerObserver,
      public ui::TableModel {
 public:
  explicit TaskManagerTableModel(TableViewDelegate* delegate);
  ~TaskManagerTableModel() override;

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column) override;
  gfx::ImageSkia GetIcon(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(int row1, int row2, int column_id) override;

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(TaskId id) override;
  void OnTaskToBeRemoved(TaskId id) override;
  void OnTasksRefreshed(const TaskIdList& task_ids) override;

  // Gets the start index and length of the group to which the task at
  // |row_index| belongs.
  void GetRowsGroupRange(int row_index, int* out_start, int* out_length);

  // Activates the browser tab associated with the process in the specified
  // |row_index|.
  void ActivateTask(int row_index);

  // Kills the process on which the task at |row_index| is running.
  void KillTask(int row_index);

  // Based on the given |visibility| and the |column_id|, a particular refresh
  // type will be enabled or disabled.
  void UpdateRefreshTypes(int column_id, bool visibility);

  // Checks if the task at |row_index| is killable.
  bool IsTaskKillable(int row_index) const;

  // Restores the saved columns settings from a previous session into
  // |columns_settings_| and updates the table view.
  void RetrieveSavedColumnsSettingsAndUpdateTable();

  // Stores the current values in |column_settings_| to the user prefs so that
  // it can be restored later next time the task manager view is opened.
  void StoreColumnsSettings();

  void ToggleColumnVisibility(int column_id);

  // Returns the row index corresponding to a particular WebContents. Returns -1
  // if |web_contents| is nullptr, or is not currently found in the model (for
  // example, if the tab is currently crashed).
  int GetRowForWebContents(content::WebContents* web_contents);

 private:
  friend class TaskManagerTester;

  // Start / stop observing the task manager.
  void StartUpdating();
  void StopUpdating();

  void OnRefresh();

  // Checks whether the task at |row_index| is the first task in its process
  // group of tasks.
  bool IsTaskFirstInGroup(int row_index) const;

  // The delegate that will be used to communicate with the platform-specific
  // TableView.
  TableViewDelegate* table_view_delegate_;

  // Contains either the column settings retrieved from user preferences if it
  // exists, or the default column settings.
  // The columns settings are the visible columns and the last sorted column
  // and the direction of the sort.
  std::unique_ptr<base::DictionaryValue> columns_settings_;

  // The table model observer that will be set by the table view of the task
  // manager.
  ui::TableModelObserver* table_model_observer_;

  // The sorted list of task IDs by process ID then by task ID.
  std::vector<TaskId> tasks_;

  // The owned task manager values stringifier that will be used to convert the
  // values to string16.
  std::unique_ptr<TaskManagerValuesStringifier> stringifier_;

  // The status of the flag #enable-nacl-debug.
  bool is_nacl_debugging_flag_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTableModel);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_
