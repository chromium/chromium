// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/task_manager_mac_table_view.h"

#include "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

@implementation TaskManagerMacTableView : NSTableView

- (void)keyDown:(NSEvent*)event {
  ui::KeyboardCode keyCode = ui::KeyboardCodeFromKeyCode(event.keyCode);
  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  NSIndexSet* selectedRows = self.selectedRowIndexes;
  size_t firstSelectedRow = selectedRows.firstIndex;
  size_t lastSelectedRow = selectedRows.lastIndex;
  size_t totalRows = self.numberOfRows;

  if (keyCode == ui::VKEY_UP && firstSelectedRow > 0) {
    [indexSet addIndex:firstSelectedRow - 1];
    [self selectRowIndexes:indexSet byExtendingSelection:NO];
  } else if (keyCode == ui::VKEY_DOWN && lastSelectedRow < totalRows - 1) {
    [indexSet addIndex:lastSelectedRow + 1];
    [self selectRowIndexes:indexSet byExtendingSelection:NO];
  } else {
    [super keyDown:event];
  }
}
@end
