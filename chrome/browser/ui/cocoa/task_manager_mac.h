// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
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
  NSTableView* tableView_;
  NSButton* endProcessButton_;
  task_manager::TaskManagerMac* taskManagerMac_;     // weak
  task_manager::TaskManagerTableModel* tableModel_;  // weak

  base::scoped_nsobject<WindowSizeAutosaver> size_saver_;

  // These contain a permutation of [0..|tableModel_->RowCount() - 1|]. Used to
  // implement sorting.
  std::vector<int> viewToModelMap_;
  std::vector<int> modelToViewMap_;

  // Descriptor of the current sort column.
  task_manager::TableSortDescriptor currentSortDescriptor_;

  // Re-entrancy flag to allow meddling with the sort descriptor.
  BOOL withinSortDescriptorsDidChange_;
}

// Creates and shows the task manager's window.
- (id)initWithTaskManagerMac:(task_manager::TaskManagerMac*)taskManagerMac
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
class TaskManagerMac : public ui::TableModelObserver,
                       public content::NotificationObserver,
                       public TableViewDelegate {
 public:
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
  void OnItemsChanged(int start, int length) override;
  void OnItemsAdded(int start, int length) override;
  void OnItemsRemoved(int start, int length) override;

  // TableViewDelegate:
  bool IsColumnVisible(int column_id) const override;
  void SetColumnVisibility(int column_id, bool new_visibility) override;
  bool IsTableSorted() const override;
  TableSortDescriptor GetSortDescriptor() const override;
  void SetSortDescriptor(const TableSortDescriptor& descriptor) override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Our model.
  TaskManagerTableModel table_model_;

  // Controller of our window, destroys itself when the task manager window
  // is closed.
  TaskManagerWindowController* window_controller_;  // weak

  content::NotificationRegistrar registrar_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to be null when the window is closed.
  static TaskManagerMac* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerMac);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
