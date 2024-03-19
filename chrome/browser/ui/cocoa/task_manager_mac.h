// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/callback_list.h"
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
                          NSMenuDelegate>

// The current sort descriptor.
@property(nonatomic) task_manager::TableSortDescriptor sortDescriptor;

// Creates and shows the task manager's window.
- (instancetype)
    initWithTaskManagerMac:(task_manager::TaskManagerMac*)taskManagerMac
                tableModel:(task_manager::TaskManagerTableModel*)tableModel;

// Refreshes all data in the task manager table.
- (void)reloadData;

// Returns YES if the specified column is visible.
- (BOOL)visibilityOfColumnWithId:(int)columnId;

// Sets the visibility of the specified column.
- (void)setVisibility:(BOOL)visibility ofColumnWithId:(int)columnId;

// Callback for "End process" button.
- (IBAction)killSelectedProcesses:(id)sender;

// Callback for double clicks on the table.
- (void)tableWasDoubleClicked:(id)sender;
@end

@interface TaskManagerWindowController (TestingAPI)
@property(readonly) NSTableView* tableViewForTesting;
@property(readonly) NSButton* endProcessButtonForTesting;
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
  bool SetColumnVisibility(int column_id, bool new_visibility) override;
  bool IsTableSorted() const override;
  TableSortDescriptor GetSortDescriptor() const override;
  void SetSortDescriptor(const TableSortDescriptor& descriptor) override;
  void MaybeHighlightActiveTask() override;

  void OnAppTerminating();

  // The model holding the data for the table.
  TaskManagerTableModel table_model_;

  // The window controller that runs the window.
  TaskManagerWindowController* __strong window_controller_;

  base::CallbackListSubscription on_app_terminating_subscription_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to be null when the window is closed.
  static TaskManagerMac* instance_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
