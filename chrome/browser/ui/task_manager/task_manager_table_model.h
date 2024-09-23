// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
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
  TableViewDelegate() = default;
  TableViewDelegate(const TableViewDelegate&) = delete;
  TableViewDelegate& operator=(const TableViewDelegate&) = delete;
  virtual ~TableViewDelegate() = default;

  virtual bool IsColumnVisible(int column_id) const = 0;

  // Returns whether the visibility has been changed. (Always return true on
  // Mac. See TaskManagerMac::toggleColumn)
  virtual bool SetColumnVisibility(int column_id, bool new_visibility) = 0;

  virtual bool IsTableSorted() const = 0;

  virtual TableSortDescriptor GetSortDescriptor() const = 0;

  virtual void SetSortDescriptor(
      const TableSortDescriptor& sort_descriptor) = 0;

  // Highlight task if no task is currently highlighted and `active_task_id_`
  // has value and is present in Task Manager, otherwise do nothing. Highlight
  // task will happen most likely right after task manager is open. We do not
  // want to override user selection if user has selected any tasks.
  virtual void MaybeHighlightActiveTask() = 0;
};

class TaskManagerTableModel : public TaskManagerObserver,
                              public ui::TableModel {
 public:
  explicit TaskManagerTableModel(TableViewDelegate* delegate);
  TaskManagerTableModel(const TaskManagerTableModel&) = delete;
  TaskManagerTableModel& operator=(const TaskManagerTableModel&) = delete;
  ~TaskManagerTableModel() override;

  // ui::TableModel:
  size_t RowCount() override;
  std::u16string GetText(size_t row, int column) override;
  ui::ImageModel GetIcon(size_t row) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(size_t row1, size_t row2, int column_id) override;

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(TaskId id) override;
  void OnTaskToBeRemoved(TaskId id) override;
  void OnTasksRefreshed(const TaskIdList& task_ids) override;
  void OnActiveTaskFetched(TaskId id) override;

  // Gets the start index and length of the group to which the task at
  // |row_index| belongs.
  void GetRowsGroupRange(size_t row_index,
                         size_t* out_start,
                         size_t* out_length);

  // Activates the browser tab associated with the process in the specified
  // |row_index|.
  void ActivateTask(size_t row_index);

  // Kills the process on which the task at |row_index| is running.
  void KillTask(size_t row_index);

  // Based on the given |visibility| and the |column_id|, a particular refresh
  // type will be enabled or disabled.
  void UpdateRefreshTypes(int column_id, bool visibility);

  // Checks if the task at |row_index| is killable.
  bool IsTaskKillable(size_t row_index) const;

  // Restores the saved columns settings from a previous session into
  // |columns_settings_| and updates the table view.
  void RetrieveSavedColumnsSettingsAndUpdateTable();

  // Stores the current values in |column_settings_| to the user prefs so that
  // it can be restored later next time the task manager view is opened.
  void StoreColumnsSettings();

  void ToggleColumnVisibility(int column_id);

  // Returns the row index corresponding to a particular WebContents. Returns
  // nullopt if |web_contents| is nullptr, or is not currently found in the
  // model (for example, if the tab is currently crashed).
  std::optional<size_t> GetRowForWebContents(
      content::WebContents* web_contents);

  std::optional<size_t> GetRowForActiveTask();

 private:
  friend class TaskManagerTester;

  // Start / stop observing the task manager.
  void StartUpdating();
  void StopUpdating();

  void OnRefresh();

  // Checks whether the task at |row_index| is the first task in its process
  // group of tasks.
  bool IsTaskFirstInGroup(size_t row_index) const;

  // The delegate that will be used to communicate with the platform-specific
  // TableView.
  raw_ptr<TableViewDelegate> table_view_delegate_;

  // Contains either the column settings retrieved from user preferences if it
  // exists, or the default column settings.
  // The columns settings are the visible columns and the last sorted column
  // and the direction of the sort.
  base::Value::Dict columns_settings_;

  // The table model observer that will be set by the table view of the task
  // manager.
  raw_ptr<ui::TableModelObserver> table_model_observer_;

  // The sorted list of task IDs by process ID then by task ID.
  std::vector<TaskId> tasks_;

  // The owned task manager values stringifier that will be used to convert the
  // values to string16.
  std::unique_ptr<TaskManagerValuesStringifier> stringifier_;

  // The status of the flag #enable-nacl-debug.
  bool is_nacl_debugging_flag_enabled_;

  // Active task id when task manager is open. This variable will only set once
  // after task manager is open. In desktop platforms other than Lacros, active
  // tab is automatically highlighted when task manager is open. But in ash and
  // Lacros it needs to wait for CROS API to passed back active task id to
  // support that. This is currently only used in tracking Lacros active tab
  // from ash through crosapi.
  std::optional<TaskId> active_task_id_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_
