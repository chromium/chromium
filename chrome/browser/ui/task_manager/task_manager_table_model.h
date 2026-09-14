// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class WebContents;
}

namespace ui {
class ColorProvider;
}

namespace task_manager {

// Do not reuse integers. This enum is required to be stable because the integer
// values are saved to the user's profile.
//
// Determines what OTHER processes to filter out from the Task List.
// For example, if the selected DisplayCategory is kTabs, Tab processes will be
// kept, and Extension and System processes will be filtered out.
enum class DisplayCategory : uint8_t {
  kAll = 0,
  kTabsAndExtensions = 1,
  kSystem = 2,
  kMax = kSystem
};

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
};

class TaskManagerTableModel : public TaskManagerObserver,
                              public ui::TableModel {
 public:
  static constexpr DisplayCategory kDefaultCategory =
      DisplayCategory::kTabsAndExtensions;

  // TODO(crbug.com/364926055): Once the refreshed Task Manager has launched,
  // change the initial display category to kDefaultCategory. Required to be
  // kAll for backwards compatibility with prod.
  explicit TaskManagerTableModel(
      TableViewDelegate* delegate,
      DisplayCategory initial_display_category = DisplayCategory::kAll);
  TaskManagerTableModel(const TaskManagerTableModel&) = delete;
  TaskManagerTableModel& operator=(const TaskManagerTableModel&) = delete;
  ~TaskManagerTableModel() override;

  // ui::TableModel:
  size_t RowCount() const override;
  std::u16string GetText(size_t row, int column) const override;
  ui::ImageModel GetIcon(size_t row) const override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(size_t row1, size_t row2, int column_id) const override;
  std::u16string GetAXNameForHeader(
      const std::vector<std::u16string>& visible_column_titles,
      const std::vector<std::u16string>& visible_column_sortable)
      const override;
  std::u16string GetAXNameForHeaderCell(
      const std::u16string& visible_column_title,
      const std::u16string& visible_column_sortable) const override;
  std::u16string GetAXNameForRow(
      size_t row,
      const std::vector<int>& visible_column_ids) const override;

  static std::u16string FormatListToString(
      base::span<const std::u16string> items);

  void FilterTaskList(TaskIdList& tasks);

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(TaskId id) override;
  void OnTaskToBeRemoved(TaskId id) override;
  void OnTasksRefreshed(const TaskIdList& task_ids) override;
  void OnTasksRefreshedWithBackgroundCalculations(
      const TaskIdList& task_ids) override;

  // Gets the start index and length of the group to which the task at
  // |row_index| belongs.
  void GetRowsGroupRange(size_t row_index,
                         size_t* out_start,
                         size_t* out_length);

  // Activates the browser tab associated with the process in the specified
  // |row_index|.
  void ActivateTask(size_t row_index);

  // Kills the process on which the task at |row_index| is running.
  // Returns true if the process terminates.
  bool KillTask(size_t row_index);

  // Based on the given |visibility| and the |column_id|, a particular refresh
  // type will be enabled or disabled.
  void UpdateRefreshTypes(int column_id, bool visibility);

  // Checks if the task at |row_index| is killable.
  bool IsTaskKillable(size_t row_index) const;

  // Restores the saved columns settings from a previous session into
  // |columns_settings_| and updates the table view.
  void RetrieveSavedColumnsSettingsAndUpdateTable(
      bool default_sorted_column_to_cpu = false);

  // Stores the current values in |column_settings_| to the user prefs so that
  // it can be restored later next time the task manager view is opened.
  void StoreColumnsSettings();

  void ToggleColumnVisibility(int column_id);

  // Returns the row index corresponding to a particular WebContents. Returns
  // nullopt if |web_contents| is nullptr, or is not currently found in the
  // model (for example, if the tab is currently crashed).
  std::optional<size_t> GetRowForWebContents(
      content::WebContents* web_contents);

  // Updates the total time spent in the provided category based on the current
  // time (base::TimeTicks::Now()).
  void UpdateOldTabTime(DisplayCategory old_category);

  // Start the timer for the new category.
  void StartNewTabTime(DisplayCategory new_category);

  // Updates task positions based on category and search filters. Returns true
  // if the model is changed.
  bool UpdateModel(const DisplayCategory display_category,
                   std::u16string_view search_term);

 private:
  friend class TaskManagerTableModelTest;
  friend class TaskManagerTester;

  // The theme colors a themeable task icon is recolored for by
  // favicon::ThemeFavicon().
  struct TaskIconThemeColors {
    static TaskIconThemeColors FromColorProvider(
        const ui::ColorProvider& color_provider);
    friend bool operator==(const TaskIconThemeColors&,
                           const TaskIconThemeColors&) = default;
    SkColor icon_color = SK_ColorTRANSPARENT;
    SkColor icon_background_color = SK_ColorTRANSPARENT;
    SkColor row_background_color = SK_ColorTRANSPARENT;
  };

  // A themeable task icon recolored by favicon::ThemeFavicon() for one set of
  // theme colors, along with the inputs it was generated from.
  struct ThemedIcon {
    gfx::ImageSkia source;
    TaskIconThemeColors colors;
    gfx::ImageSkia themed;
  };

  // Rasterizes the ui::ImageModel that GetIcon() returns for a themeable
  // icon: |icon|, the icon of the task with |task_id|, recolored for the theme
  // of |color_provider|. |model| is the model the icon belongs to, or null once
  // it has been destroyed. It caches the result so the color analysis behind
  // the recoloring doesn't rerun on every paint.
  static gfx::ImageSkia RasterizeThemedIcon(
      base::WeakPtr<const TaskManagerTableModel> model,
      TaskId task_id,
      const gfx::ImageSkia& icon,
      const ui::ColorProvider* color_provider);

  // Start / stop observing the task manager.
  void StartUpdating();
  void StopUpdating();

  void OnRefresh(const TaskIdList& task_ids);

  // Checks whether the task at |row_index| is the first task in its process
  // group of tasks.
  bool IsTaskFirstInGroup(size_t row_index) const;

  // Retrieves task types for `child_task_id`. If it has a parent, its parent
  // task types are retrieved instead.
  //
  // Returns true if the `out_*` parameters were populated successfully, and
  // false if types could not be retrieved from the root or `child_task_id`.
  bool FetchTaskTypes(TaskId child_task_id,
                      Task::Type& out_type,
                      Task::SubType& out_subtype) const;

  // Checks whether the task falls in `Tabs`, `Extensions` or `Systems`
  // category.
  bool ShouldKeepTaskForSupportedType(TaskId task_id) const;

  // Determines whether a TaskId should be kept based on the DisplayCategory.
  bool ShouldKeepTask(TaskId task_id) const;

  // Returns whether `task_id` related task group has matching tasks in current
  // task list.
  bool HasMatchInTasksSharingSameProcess(TaskId task_id) const;

  // Goes through the task list to get matched process ids based on search
  // terms.
  void UpdateMatchedProcessSet();

  // Updates matched process set by single task based on search terms.
  void UpdateMatchedProcessSetById(TaskId task_id);

  // The delegate that will be used to communicate with the platform-specific
  // TableView.
  raw_ptr<TableViewDelegate> table_view_delegate_;

  // Contains either the column settings retrieved from user preferences if it
  // exists, or the default column settings.
  // The columns settings are the visible columns and the last sorted column
  // and the direction of the sort.
  base::DictValue columns_settings_;

  // The table model observer that will be set by the table view of the task
  // manager.
  raw_ptr<ui::TableModelObserver> table_model_observer_;

  // The sorted list of task IDs by process ID then by task ID.
  std::vector<TaskId> tasks_;

  // The owned task manager values stringifier that will be used to convert the
  // values to string16.
  std::unique_ptr<TaskManagerValuesStringifier> stringifier_;

  // Determines which rows should be kept from GetTaskIdsList().
  DisplayCategory display_category_;

  // Search keyword to filter tasks.
  std::u16string search_terms_;

  // Contains the process IDs for tasks whose titles match the search
  // terms. Tasks linked to these processes should be kept.
  std::unordered_set<base::ProcessId> matched_process_set_;

  // Stores the total time spent in the category for this Task Manager session.
  base::TimeTicks tabs_and_ex_start_time_ = base::TimeTicks::Now();
  base::TimeTicks system_start_time_ = base::TimeTicks::Now();
  base::TimeTicks all_start_time_ = base::TimeTicks::Now();

  base::TimeDelta tabs_and_ex_total_time_;
  base::TimeDelta system_total_time_;
  base::TimeDelta all_total_time_;

  // Themeable icons recolored for the current theme, keyed by task id. Filled
  // lazily by RasterizeThemedIcon(); an entry is regenerated when its task's
  // icon or the theme colors change, and dropped when the task is removed.
  // Mutable: it is a cache filled while rasterizing the icons handed out by
  // GetIcon(), a const accessor.
  mutable base::flat_map<TaskId, ThemedIcon> themed_icons_;

  base::WeakPtrFactory<TaskManagerTableModel> weak_ptr_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_TABLE_MODEL_H_
