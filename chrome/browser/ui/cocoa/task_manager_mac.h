// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/callback_list.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "ui/base/models/table_model_observer.h"

@class WindowSizeAutosaver;

namespace task_manager {
class TaskManagerMac;
}

// This class is responsible for loading the task manager window and for
// managing it.
@interface TaskManagerWindowController
    : NSWindowController <NSWindowDelegate,
                          NSTableViewDataSource,
                          NSTableViewDelegate,
                          NSMenuDelegate> {
 @private
  NSTableView* _tableView;
  NSButton* _endProcessButton;
  raw_ptr<task_manager::TaskManagerMac, DanglingUntriaged>
      _taskManagerMac;  // weak
  raw_ptr<task_manager::TaskManagerTableModel, DanglingUntriaged>
      _tableModel;  // weak

  base::scoped_nsobject<WindowSizeAutosaver> _size_saver;

  // These contain a permutation of [0..|tableModel_->RowCount() - 1|]. Used to
  // implement sorting.
  std::vector<size_t> _viewToModelMap;
  std::vector<size_t> _modelToViewMap;

  // Descriptor of the current sort column.
  task_manager::TableSortDescriptor _currentSortDescriptor;

  // Re-entrancy flag to allow meddling with the sort descriptor.
  BOOL _withinSortDescriptorsDidChange;
}

// Creates and shows the task manager's window.
- (instancetype)
    initWithTaskManagerMac:(task_manager::TaskManagerMac*)taskManagerMac
                tableModel:(task_manager::TaskManagerTableModel*)tableModel;

// Refreshes all data in the task manager table.
- (void)reloadData;

// Gets a copy of the current sort descriptor.
- (task_manager::TableSortDescriptor)sortDescriptor;

// Sets the current sort descriptor.
- (void)setSortDescriptor:
    (const task_manager::TableSortDescriptor&)sortDescriptor;

// Returns YES if the specified column is visible.
- (BOOL)visibilityOfColumnWithId:(int)columnId;

// Sets the visibility of the specified column.
- (void)setColumnWithId:(int)columnId toVisibility:(BOOL)visibility;

// Callback for "End process" button.
- (IBAction)killSelectedProcesses:(id)sender;

// Callback for double clicks on the table.
- (void)tableWasDoubleClicked:(id)sender;
@end

@interface TaskManagerWindowController (TestingAPI)
- (NSTableView*)tableViewForTesting;
- (NSButton*)endProcessButtonForTesting;
@end

namespace task_manager {

// This class runs the Task Manager on the Mac.
class TaskManagerMac : public ui::TableModelObserver, public TableViewDelegate {
 public:
  TaskManagerMac(const TaskManagerMac&) = delete;
  TaskManagerMac& operator=(const TaskManagerMac&) = delete;

  // Called by the TaskManagerWindowController:
  void WindowWasClosed();
  NSImage* GetImageForRow(int row);

  // Creates the task manager if it doesn't exist; otherwise, it activates the
  // existing task manager window.
  static TaskManagerTableModel* Show();

  // Hides the task manager if it is showing.
  static void Hide();

  // Various test-only functions.
  static TaskManagerMac* GetInstanceForTests() { return instance_; }
  TaskManagerTableModel* GetTableModelForTests() { return &table_model_; }
  TaskManagerWindowController* CocoaControllerForTests() {
    return window_controller_;
  }

 private:
  TaskManagerMac();
  ~TaskManagerMac() override;

  // ui::TableModelObserver:
  void OnModelChanged() override;
  void OnItemsChanged(size_t start, size_t length) override;
  void OnItemsAdded(size_t start, size_t length) override;
  void OnItemsRemoved(size_t start, size_t length) override;

  // TableViewDelegate:
  bool IsColumnVisible(int column_id) const override;
  void SetColumnVisibility(int column_id, bool new_visibility) override;
  bool IsTableSorted() const override;
  TableSortDescriptor GetSortDescriptor() const override;
  void SetSortDescriptor(const TableSortDescriptor& descriptor) override;

  void OnAppTerminating();

  // Our model.
  TaskManagerTableModel table_model_;

  // Controller of our window, destroys itself when the task manager window
  // is closed.
  TaskManagerWindowController* window_controller_;  // weak

  base::CallbackListSubscription on_app_terminating_subscription_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to be null when the window is closed.
  static TaskManagerMac* instance_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
