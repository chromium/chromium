// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/cocoa/task_manager_mac.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/cocoa/task_manager_mac_table_view.h"
#import "chrome/browser/ui/cocoa/window_size_autosaver.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

NSString* ColumnIdentifier(int id) {
  return [NSString stringWithFormat:@"%d", id];
}

}  // namespace

@interface TaskManagerWindowController ()

@property(readonly) std::vector<size_t> modelSelection;

- (void)sortShuffleArray;
- (void)reloadDataWithModelSelection:(std::vector<size_t>)modelSelection;
- (void)reloadDataWithRowsAdded:(size_t)addedRows
                   addedAtIndex:(size_t)addedRowIndex;
- (void)reloadDataWithRowsRemoved:(size_t)removedRows
                   removedAtIndex:(size_t)removedRowIndex;
- (NSWindow*)createAndLayOutWindow;
- (NSTableColumn*)addColumnWithData:
    (const task_manager::TableColumnData&)columnData;
- (void)setUpTableColumns;
- (void)setUpTableHeaderContextMenu;
- (void)toggleColumn:(NSMenuItem*)item;
- (void)adjustSelectionAndEndProcessButton;
- (void)deselectRows;

@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWindowController implementation:

@implementation TaskManagerWindowController {
  NSTableView* __strong _tableView;
  NSButton* __strong _endProcessButton;
  raw_ptr<task_manager::TaskManagerMac, DanglingUntriaged>
      _taskManagerMac;  // weak
  raw_ptr<task_manager::TaskManagerTableModel, DanglingUntriaged>
      _tableModel;  // weak

  WindowSizeAutosaver* __strong _size_saver;

  // These contain a permutation of [0..|tableModel_->RowCount() - 1|]. Used to
  // implement sorting.
  std::vector<size_t> _viewToModelMap;
  std::vector<size_t> _modelToViewMap;

  // Descriptor of the current sort column.
  task_manager::TableSortDescriptor _currentSortDescriptor;

  // Re-entrancy flag to allow meddling with the sort descriptor.
  BOOL _withinSortDescriptorsDidChange;
}

- (instancetype)
    initWithTaskManagerMac:(task_manager::TaskManagerMac*)taskManagerMac
                tableModel:(task_manager::TaskManagerTableModel*)tableModel {
  NSWindow* window = [self createAndLayOutWindow];
  if ((self = [super initWithWindow:window])) {
    _taskManagerMac = taskManagerMac;
    _tableModel = tableModel;

    window.delegate = self;

    _tableView.delegate = self;
    _tableView.dataSource = self;

    if (g_browser_process && g_browser_process->local_state()) {
      _size_saver = [[WindowSizeAutosaver alloc]
          initWithWindow:self.window
             prefService:g_browser_process->local_state()
                    path:prefs::kTaskManagerWindowPlacement];
    }
    self.window.excludedFromWindowsMenu = YES;

    [self setUpTableColumns];
    [self setUpTableHeaderContextMenu];
    [self adjustSelectionAndEndProcessButton];
    [_tableView sizeToFit];

    [self reloadData];
    [self showWindow:self];
  }
  return self;
}

- (void)sortShuffleArray {
  _viewToModelMap.resize(_tableModel->RowCount());
  for (size_t i = 0; i < _viewToModelMap.size(); ++i)
    _viewToModelMap[i] = i;

  if (_currentSortDescriptor.sorted_column_id != -1) {
    task_manager::TaskManagerTableModel* tableModel = _tableModel;
    task_manager::TableSortDescriptor currentSortDescriptor =
        _currentSortDescriptor;
    std::stable_sort(_viewToModelMap.begin(), _viewToModelMap.end(),
                     [tableModel, currentSortDescriptor](int a, int b) {
                       size_t aStart, aLength;
                       tableModel->GetRowsGroupRange(a, &aStart, &aLength);
                       size_t bStart, bLength;
                       tableModel->GetRowsGroupRange(b, &bStart, &bLength);
                       if (aStart == bStart) {
                         // The two rows are in the same group, sort so that
                         // items in the same group always appear in the same
                         // order. The sort descriptor's ascending value is
                         // intentionally ignored.
                         return a < b;
                       }

                       // Sort by the first entry of each of the groups.
                       int cmp_result = tableModel->CompareValues(
                           aStart, bStart,
                           currentSortDescriptor.sorted_column_id);
                       if (!currentSortDescriptor.is_ascending)
                         cmp_result = -cmp_result;
                       return cmp_result < 0;
                     });
  }

  _modelToViewMap.resize(_viewToModelMap.size());
  for (size_t i = 0; i < _viewToModelMap.size(); ++i)
    _modelToViewMap[_viewToModelMap[i]] = i;
}

- (void)reloadData {
  [self reloadDataWithRowsAdded:0 addedAtIndex:0];
}

- (std::vector<size_t>)modelSelection {
  NSIndexSet* viewSelection = _tableView.selectedRowIndexes;
  std::vector<size_t> modelSelection;
  for (NSUInteger i = viewSelection.lastIndex; i != NSNotFound;
       i = [viewSelection indexLessThanIndex:i]) {
    modelSelection.push_back(_viewToModelMap[i]);
  }
  return modelSelection;
}

- (void)reloadDataWithModelSelection:(std::vector<size_t>)modelSelection {
  // Sort.
  [self sortShuffleArray];

  // Reload the data.
  [_tableView reloadData];

  // Reload the selection.
  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  for (auto selectedItem : modelSelection)
    [indexSet addIndex:_modelToViewMap[selectedItem]];
  [_tableView selectRowIndexes:indexSet byExtendingSelection:NO];

  [self adjustSelectionAndEndProcessButton];
}

- (void)reloadDataWithRowsAdded:(size_t)addedRows
                   addedAtIndex:(size_t)addedRowIndex {
  std::vector<size_t> modelSelection = self.modelSelection;

  // Adjust for any added rows.
  for (size_t& selectedItem : modelSelection) {
    if (selectedItem >= addedRowIndex)
      selectedItem += addedRows;
  }

  [self reloadDataWithModelSelection:std::move(modelSelection)];
}

- (void)reloadDataWithRowsRemoved:(size_t)removedRows
                   removedAtIndex:(size_t)removedRowIndex {
  std::vector<size_t> modelSelection = self.modelSelection;

  // Adjust for any removed rows.
  std::vector<size_t> newModelSelection;
  for (size_t selectedItem : modelSelection) {
    if (selectedItem < removedRowIndex)
      newModelSelection.push_back(selectedItem);
    else if (selectedItem >= removedRowIndex + removedRows)
      newModelSelection.push_back(selectedItem - removedRows);
  }

  [self reloadDataWithModelSelection:std::move(newModelSelection)];
}

- (task_manager::TableSortDescriptor)sortDescriptor {
  return _currentSortDescriptor;
}

- (void)setSortDescriptor:(task_manager::TableSortDescriptor)sortDescriptor {
  NSSortDescriptor* nsSortDescriptor = [[NSSortDescriptor alloc]
      initWithKey:ColumnIdentifier(sortDescriptor.sorted_column_id)
        ascending:sortDescriptor.is_ascending];
  [_tableView setSortDescriptors:@[ nsSortDescriptor ]];
}

- (BOOL)visibilityOfColumnWithId:(int)columnId {
  NSTableColumn* column =
      [_tableView tableColumnWithIdentifier:ColumnIdentifier(columnId)];
  return column ? !column.hidden : NO;
}

- (void)setVisibility:(BOOL)visibility ofColumnWithId:(int)columnId {
  NSTableColumn* column =
      [_tableView tableColumnWithIdentifier:ColumnIdentifier(columnId)];
  column.hidden = !visibility;

  [_tableView sizeToFit];
  [_tableView setNeedsDisplay:YES];
}

- (IBAction)killSelectedProcesses:(id)sender {
  NSIndexSet* selection = _tableView.selectedRowIndexes;
  for (NSUInteger i = selection.lastIndex; i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    _tableModel->KillTask(_viewToModelMap[i]);
  }
}

- (void)tableWasDoubleClicked:(id)sender {
  NSInteger row = _tableView.clickedRow;
  if (row < 0)
    return;  // Happens e.g. if the table header is double-clicked.
  _tableModel->ActivateTask(_viewToModelMap[row]);
}

- (void)dealloc {
  // Paranoia. These should have been nilled out in -windowWillClose: but let's
  // make sure we have no dangling references.
  _tableView.delegate = nil;
  _tableView.dataSource = nil;
}

// Creates a NSWindow for the task manager and lays out the views inside the
// content view.
- (NSWindow*)createAndLayOutWindow {
  static constexpr CGFloat kWindowWidth = 480;
  static constexpr CGFloat kMargin = 20;

  NSWindow* window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(195, 240, kWindowWidth, 270)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:YES];
  window.minSize = NSMakeSize(300, 200);
  window.title = l10n_util::GetNSString(IDS_TASK_MANAGER_TITLE);

  NSView* contentView = window.contentView;

  // Create the button that terminates the selected process in the table.
  _endProcessButton =
      [NSButton buttonWithTitle:l10n_util::GetNSString(IDS_TASK_MANAGER_KILL)
                         target:self
                         action:@selector(killSelectedProcesses:)];
  _endProcessButton.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
  [_endProcessButton sizeToFit];
  NSRect buttonFrame = _endProcessButton.frame;
  buttonFrame.size.width += kMargin;
  // Adjust the button's origin so that it is flush with the right-hand side of
  // the table.
  buttonFrame.origin.x =
      NSWidth(contentView.frame) - NSWidth(buttonFrame) - kMargin + 6;
  // Use only half the margin, since the full margin is too much whitespace.
  buttonFrame.origin.y = kMargin / 2;
  _endProcessButton.frame = buttonFrame;
  _endProcessButton.keyEquivalent = @"\r";
  [contentView addSubview:_endProcessButton];

  // Create a scroll view to house the table view.
  CGFloat scrollViewY = NSMaxY(buttonFrame) + kMargin / 2;
  NSRect scrollViewFrame =
      NSMakeRect(kMargin, scrollViewY, kWindowWidth - 2 * kMargin,
                 NSHeight(window.frame) - 2 * kMargin - scrollViewY);
  NSScrollView* scrollView =
      [[NSScrollView alloc] initWithFrame:scrollViewFrame];
  scrollView.autoresizesSubviews = YES;
  scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  scrollView.borderType = NSBezelBorder;
  scrollView.focusRingType = NSFocusRingTypeNone;
  scrollView.hasVerticalScroller = YES;
  scrollView.verticalScroller.controlSize = NSControlSizeSmall;
  [contentView addSubview:scrollView];

  // Create the table view. The data source and delegate are connected in
  // the designated initializer.
  TaskManagerMacTableView* tableView = [[TaskManagerMacTableView alloc]
      initWithFrame:NSMakeRect(0, 0, 400, 200)];
  tableView.allowsColumnReordering = NO;
  tableView.allowsMultipleSelection = YES;
  // No autosaving, since column identifiers are IDS_ values which are not
  // stable. TODO(avi): Would it be worth it to find stable identifiers so that
  // we could use autosaving?
  tableView.autosaveTableColumns = NO;
  tableView.columnAutoresizingStyle = NSTableViewUniformColumnAutoresizingStyle;
  tableView.doubleAction = @selector(tableWasDoubleClicked:);
  tableView.focusRingType = NSFocusRingTypeNone;
  tableView.intercellSpacing = NSMakeSize(0, 0);
  tableView.usesAlternatingRowBackgroundColors = YES;
  _tableView = tableView;

  scrollView.documentView = tableView;

  return window;
}

// Adds a column which has the given string id as title. |isVisible| specifies
// if the column is initially visible.
- (NSTableColumn*)addColumnWithData:
    (const task_manager::TableColumnData&)columnData {
  NSTableColumn* column = [[NSTableColumn alloc]
      initWithIdentifier:ColumnIdentifier(columnData.id)];

  NSTableHeaderCell* headerCell = column.headerCell;
  NSCell* dataCell = column.dataCell;

  NSTextAlignment textAlignment;
  // There are no "leading" and "trailing" constants in `NSTextAlignment` so do
  // it manually.
  if (NSApp.userInterfaceLayoutDirection ==
      NSUserInterfaceLayoutDirectionRightToLeft) {
    textAlignment = (columnData.align == ui::TableColumn::LEFT)
                        ? NSTextAlignmentRight
                        : NSTextAlignmentLeft;
  } else {
    textAlignment = (columnData.align == ui::TableColumn::LEFT)
                        ? NSTextAlignmentLeft
                        : NSTextAlignmentRight;
  }
  headerCell.alignment = textAlignment;
  dataCell.alignment = textAlignment;

  headerCell.stringValue = l10n_util::GetNSStringWithFixup(columnData.id);

  dataCell.font =
      [NSFont monospacedDigitSystemFontOfSize:NSFont.smallSystemFontSize
                                       weight:NSFontWeightRegular];

  column.hidden = !columnData.default_visibility;
  column.editable = NO;

  NSSortDescriptor* sortDescriptor = [[NSSortDescriptor alloc]
      initWithKey:ColumnIdentifier(columnData.id)
        ascending:columnData.initial_sort_is_ascending];
  [column setSortDescriptorPrototype:sortDescriptor];

  column.minWidth = columnData.min_width;
  // If there is no specified max width, use a reasonable value of 1.5x the min
  // width, but make sure that the max width is big enough to actually show the
  // entire column title.
  const int kTitleMargin = 40;  // Space for the arrow, etc.
  int maxWidth = columnData.max_width;
  if (maxWidth <= 0) {
    maxWidth = 3 * columnData.min_width / 2;
  }
  int columnTitleWidth =
      [headerCell.stringValue
          sizeWithAttributes:@{NSFontAttributeName : headerCell.font}]
          .width +
      kTitleMargin;
  maxWidth = std::max(maxWidth, columnTitleWidth);
  column.maxWidth = maxWidth;
  column.resizingMask =
      NSTableColumnAutoresizingMask | NSTableColumnUserResizingMask;

  [_tableView addTableColumn:column];
  return column;
}

// Adds all the task manager's columns to the table.
- (void)setUpTableColumns {
  for (NSTableColumn* column in _tableView.tableColumns) {
    [_tableView removeTableColumn:column];
  }

  for (size_t i = 0; i < task_manager::kColumnsSize; ++i) {
    const auto& columnData = task_manager::kColumns[i];
    NSTableColumn* column = [self addColumnWithData:columnData];

    if (columnData.id == IDS_TASK_MANAGER_TASK_COLUMN) {
      // The task column displays an icon for every row, done by an
      // NSButtonCell.
      NSCell* currentCell = column.dataCell;
      NSButtonCell* nameCell = [[NSButtonCell alloc] initTextCell:@""];
      nameCell.imagePosition = NSImageLeft;
      nameCell.buttonType = NSButtonTypeSwitch;
      nameCell.alignment = currentCell.alignment;
      nameCell.font = currentCell.font;
      column.dataCell = nameCell;
    }
  }
}

// Creates a context menu for the table header that allows the user to toggle
// which columns should be shown and which should be hidden (like the Activity
// Monitor.app's table header context menu).
- (void)setUpTableHeaderContextMenu {
  NSMenu* contextMenu =
      [[NSMenu alloc] initWithTitle:@"Task Manager context menu"];
  contextMenu.delegate = self;
  _tableView.headerView.menu = contextMenu;
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];

  for (NSTableColumn* column in _tableView.tableColumns) {
    NSMenuItem* item = [menu addItemWithTitle:column.headerCell.stringValue
                                       action:@selector(toggleColumn:)
                                keyEquivalent:@""];
    item.target = self;
    item.representedObject = column;
    item.state = column.hidden ? NSControlStateValueOff : NSControlStateValueOn;
  }
}

// Callback for the table header context menu. Toggles visibility of the table
// column associated with the clicked menu item.
- (void)toggleColumn:(NSMenuItem*)item {
  CHECK([item isKindOfClass:[NSMenuItem class]]);
  if (![item isKindOfClass:[NSMenuItem class]])
    return;

  NSTableColumn* column = item.representedObject;
  int columnId = column.identifier.intValue;
  CHECK(column);
  NSInteger oldState = item.state;
  NSInteger newState = oldState == NSControlStateValueOn
                           ? NSControlStateValueOff
                           : NSControlStateValueOn;

  // If hiding the column, make sure at least one column will remain visible.
  if (newState == NSControlStateValueOff) {
    // Find the first column that will be visible after hiding |column|.
    NSTableColumn* firstRemainingVisibleColumn = nil;

    for (NSTableColumn* nextColumn in _tableView.tableColumns) {
      if (nextColumn != column && !nextColumn.hidden) {
        firstRemainingVisibleColumn = nextColumn;
        break;
      }
    }

    // If no column will be visible, abort the toggle. This will basically cause
    // the toggle operation to silently fail. The other way to ensure at least
    // one visible column is to disable the menu item corresponding to the last
    // remaining visible column. That would place the menu in a weird state to
    // the user, where there's one item somewhere that's grayed out with no
    // clear explanation of why. It will be rare for a user to try hiding all
    // columns, but we still want to guard against it. If they are really intent
    // on hiding the last visible column (perhaps they plan to choose another
    // one after that to be visible), odds are they will try making another
    // column visible and then hiding the one column that would not hide.
    if (firstRemainingVisibleColumn == nil) {
      return;
    }

    // If |column| is being used to sort the table (i.e. it's the primary sort
    // column), make the first remaining visible column the new primary sort
    // column.
    int primarySortColumnId = _currentSortDescriptor.sorted_column_id;
    DCHECK(primarySortColumnId);

    if (primarySortColumnId == columnId) {
      NSSortDescriptor* newSortDescriptor =
          [firstRemainingVisibleColumn sortDescriptorPrototype];
      [_tableView setSortDescriptors:@[ newSortDescriptor ]];
    }
  }

  // Make the change. (This will call back into the SetColumnVisibility()
  // function to actually do the visibility change.)
  _tableModel->ToggleColumnVisibility(columnId);
}

// This function appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustSelectionAndEndProcessButton {
  bool allSelectionRowsAreKillableTasks = true;
  NSMutableIndexSet* groupIndexes = [NSMutableIndexSet indexSet];

  NSIndexSet* selection = _tableView.selectedRowIndexes;
  for (NSUInteger i = selection.lastIndex; i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    int modelIndex = _viewToModelMap[i];

    if (!_tableModel->IsTaskKillable(modelIndex))
      allSelectionRowsAreKillableTasks = false;

    size_t groupStart, groupLength;
    _tableModel->GetRowsGroupRange(modelIndex, &groupStart, &groupLength);
    for (size_t j = 0; j < groupLength; ++j)
      [groupIndexes addIndex:_modelToViewMap[groupStart + j]];
  }

  [_tableView selectRowIndexes:groupIndexes byExtendingSelection:YES];

  bool enabled = selection.count > 0 && allSelectionRowsAreKillableTasks &&
                 task_manager::TaskManagerInterface::IsEndProcessEnabled();
  [_endProcessButton setEnabled:enabled];
}

- (void)deselectRows {
  [_tableView deselectAll:self];
}

// Table view delegate methods.

// The selection is being changed by mouse (drag/click).
- (void)tableViewSelectionIsChanging:(NSNotification*)aNotification {
  [self adjustSelectionAndEndProcessButton];
}

// The selection is being changed by keyboard (arrows).
- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [self adjustSelectionAndEndProcessButton];
}

- (void)windowWillClose:(NSNotification*)notification {
  if (_taskManagerMac) {
    _tableModel->StoreColumnsSettings();
    _tableModel = nullptr;

    // Now that there is no model, ensure that this object gets no data requests
    // in the window of time between the release and the actual dealloc.
    // https://crbug.com/763367
    _tableView.delegate = nil;
    _tableView.dataSource = nil;

    // The _taskManagerMac holds the owning reference to this object, so notify
    // it about the closure last.
    auto localTaskManagerMac = _taskManagerMac;
    _taskManagerMac = nullptr;
    localTaskManagerMac->WindowWasClosed();
    // Do nothing else after this.
  }
}

@end

@implementation TaskManagerWindowController (NSTableDataSource)

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  DCHECK(tableView == _tableView || _tableView == nil);
  return _tableModel->RowCount();
}

- (CGFloat)tableView:(NSTableView*)tableView heightOfRow:(NSInteger)row {
  return 16;
}

- (NSString*)modelTextForRow:(int)row column:(int)columnId {
  DCHECK_LT(static_cast<size_t>(row), _viewToModelMap.size());
  return base::SysUTF16ToNSString(
      _tableModel->GetText(_viewToModelMap[row], columnId));
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  // NSButtonCells expect an on/off state as objectValue. Their title is set
  // in |tableView:dataCellForTableColumn:row:| below.
  if (tableColumn.identifier.intValue == IDS_TASK_MANAGER_TASK_COLUMN) {
    return [NSNumber numberWithInt:NSControlStateValueOff];
  }

  return [self modelTextForRow:rowIndex column:tableColumn.identifier.intValue];
}

- (NSCell*)tableView:(NSTableView*)tableView
    dataCellForTableColumn:(NSTableColumn*)tableColumn
                       row:(NSInteger)rowIndex {
  NSCell* cell = [tableColumn dataCellForRow:rowIndex];

  // Set the favicon and title for the task in the name column.
  if (tableColumn.identifier.intValue == IDS_TASK_MANAGER_TASK_COLUMN) {
    DCHECK([cell isKindOfClass:[NSButtonCell class]]);
    NSButtonCell* buttonCell = static_cast<NSButtonCell*>(cell);

    NSString* title = [self modelTextForRow:rowIndex
                                     column:tableColumn.identifier.intValue];
    NSColor* textColor = [tableView isRowSelected:rowIndex]
                             ? NSColor.alternateSelectedControlTextColor
                             : NSColor.labelColor;
    NSAttributedString* attributedTitle = [[NSAttributedString alloc]
        initWithString:title
            attributes:@{
              NSForegroundColorAttributeName : textColor,
              NSFontAttributeName : cell.font
            }];
    buttonCell.attributedTitle = attributedTitle;

    buttonCell.image =
        _taskManagerMac->GetImageForRow(_viewToModelMap[rowIndex]);
    buttonCell.refusesFirstResponder = YES;  // Don't push in like a button.
    buttonCell.highlightsBy = NSNoCellMask;
  }

  return cell;
}

- (void)tableView:(NSTableView*)tableView
    sortDescriptorsDidChange:(NSArray*)oldDescriptors {
  if (_withinSortDescriptorsDidChange)
    return;

  NSSortDescriptor* oldDescriptor = oldDescriptors.firstObject;
  NSSortDescriptor* newDescriptor = tableView.sortDescriptors.firstObject;

  // Implement three-way sorting, toggling "unsorted" as a third option.
  if (oldDescriptor && newDescriptor &&
      [oldDescriptor.key isEqual:newDescriptor.key]) {
    // The user clicked to change the sort on the previously sorted column.
    // AppKit toggled the sort order. However, if the sort was toggled to become
    // the initial sorting direction, clear it instead.
    NSTableColumn* column = [tableView
        tableColumnWithIdentifier:ColumnIdentifier(newDescriptor.key.intValue)];
    NSSortDescriptor* initialDescriptor = [column sortDescriptorPrototype];
    if (newDescriptor.ascending == initialDescriptor.ascending) {
      _withinSortDescriptorsDidChange = YES;
      [_tableView setSortDescriptors:@[]];
      newDescriptor = nil;
      _withinSortDescriptorsDidChange = NO;
    }
  }

  if (newDescriptor) {
    _currentSortDescriptor.sorted_column_id = newDescriptor.key.intValue;
    _currentSortDescriptor.is_ascending = newDescriptor.ascending;
  } else {
    _currentSortDescriptor.sorted_column_id = -1;
  }

  [self reloadData];  // Sorts.
}

@end

@implementation TaskManagerWindowController (TestingAPI)

- (NSTableView*)tableViewForTesting {
  return _tableView;
}

- (NSButton*)endProcessButtonForTesting {
  return _endProcessButton;
}

@end

namespace task_manager {

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac implementation:

TaskManagerMac::TaskManagerMac()
    : table_model_(this),
      window_controller_([[TaskManagerWindowController alloc]
          initWithTaskManagerMac:this
                      tableModel:&table_model_]) {
  table_model_.SetObserver(this);  // Hook up the ui::TableModelObserver.
  table_model_.RetrieveSavedColumnsSettingsAndUpdateTable();

  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &TaskManagerMac::OnAppTerminating, base::Unretained(this)));
}

// static
TaskManagerMac* TaskManagerMac::instance_ = nullptr;

TaskManagerMac::~TaskManagerMac() {
  table_model_.SetObserver(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// ui::TableModelObserver implementation:

void TaskManagerMac::OnModelChanged() {
  [window_controller_ deselectRows];
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsChanged(size_t start, size_t length) {
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsAdded(size_t start, size_t length) {
  [window_controller_ reloadDataWithRowsAdded:length addedAtIndex:start];
}

void TaskManagerMac::OnItemsRemoved(size_t start, size_t length) {
  [window_controller_ reloadDataWithRowsRemoved:length removedAtIndex:start];
}

////////////////////////////////////////////////////////////////////////////////
// TableViewDelegate implementation:

bool TaskManagerMac::IsColumnVisible(int column_id) const {
  return [window_controller_ visibilityOfColumnWithId:column_id];
}

bool TaskManagerMac::SetColumnVisibility(int column_id, bool new_visibility) {
  [window_controller_ setVisibility:new_visibility ofColumnWithId:column_id];
  return true;
}

bool TaskManagerMac::IsTableSorted() const {
  return window_controller_.sortDescriptor.sorted_column_id != -1;
}

TableSortDescriptor TaskManagerMac::GetSortDescriptor() const {
  return window_controller_.sortDescriptor;
}

void TaskManagerMac::SetSortDescriptor(const TableSortDescriptor& descriptor) {
  window_controller_.sortDescriptor = descriptor;
}

void TaskManagerMac::MaybeHighlightActiveTask() {}

////////////////////////////////////////////////////////////////////////////////
// Called by the TaskManagerWindowController:

void TaskManagerMac::WindowWasClosed() {
  delete this;
  instance_ = nullptr;  // |instance_| is static
}

NSImage* TaskManagerMac::GetImageForRow(int row) {
  const NSSize kImageSize = NSMakeSize(16.0, 16.0);
  NSImage* image =
      gfx::NSImageFromImageSkia(table_model_.GetIcon(row).Rasterize(nullptr));
  if (image) {
    image.size = kImageSize;
  } else {
    image = [[NSImage alloc] initWithSize:kImageSize];
  }

  return image;
}

void TaskManagerMac::OnAppTerminating() {
  Hide();
}

// static
TaskManagerTableModel* TaskManagerMac::Show() {
  if (instance_) {
    [instance_->window_controller_.window
        makeKeyAndOrderFront:instance_->window_controller_];
  } else {
    instance_ = new TaskManagerMac();
  }

  return &instance_->table_model_;
}

// static
void TaskManagerMac::Hide() {
  if (instance_)
    [instance_->window_controller_ close];
}

}  // namespace task_manager

namespace chrome {

// Declared in browser_dialogs.h.
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser) {
  return task_manager::TaskManagerMac::Show();
}

void HideTaskManager() {
  task_manager::TaskManagerMac::Hide();
}

}  // namespace chrome
