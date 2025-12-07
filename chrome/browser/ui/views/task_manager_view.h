// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_

#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/task_manager_metrics_recorder.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/task_manager_search_bar_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/table_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/controls/textfield/textfield.h"
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
                        public views::TabbedPaneListener,
                        public views::TableGrouper,
                        public views::TableViewObserver,
                        public views::ContextMenuController,
                        public ui::SimpleMenuModel::Delegate,
                        public TaskManagerSearchBarView::Delegate {
  METADATA_HEADER(TaskManagerView, views::DialogDelegateView)

 public:
  struct FilterTab {
    DisplayCategory associated_category;
    int title_id;
    // This field is not a raw_ptr<> because it only ever points to statically-
    // allocated data which is never freed, and hence cannot dangle.
    RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  };

  TaskManagerView(const TaskManagerView&) = delete;
  TaskManagerView& operator=(const TaskManagerView&) = delete;
  ~TaskManagerView() override;

  // Shows the Task Manager window, or re-activates an existing one.
  static task_manager::TaskManagerTableModel* Show(
      Browser* browser,
      StartAction start_action = StartAction::kOther);

  // Hides the Task Manager if it is showing.
  static void Hide();

  // task_manager::TableViewDelegate:
  bool IsColumnVisible(int column_id) const override;
  bool SetColumnVisibility(int column_id, bool new_visibility) override;
  bool IsTableSorted() const override;
  TableSortDescriptor GetSortDescriptor() const override;
  void SetSortDescriptor(const TableSortDescriptor& descriptor) override;

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

  // WidgetDelegate:
  void OnWidgetInitialized() override;

  // views::TableGrouper:
  void GetGroupRange(size_t model_index, views::GroupRange* range) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnKeyDown(ui::KeyboardCode keycode) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override;
  bool IsCommandIdEnabled(int id) const override;
  void ExecuteCommand(int id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // TaskManagerSearchBarView::Delegate:
  void SearchBarOnInputChanged(std::u16string_view text) override;

  views::TableView* tab_table_for_testing() { return tab_table_; }

  static TaskManagerView* GetInstanceForTests();

 private:
  // Used for the TaskManagerDesktopRefresh.
  // Determines how the UI for the TaskManager is rendered. Each boolean
  // controls a specific deviation from the original TaskManager UI.
  // TODO(crbug.com/364926055): Remove after feature is enabled by default.
  struct TableConfigs {
    bool table_has_border;
    bool header_style;
    bool table_refresh;
    bool scroll_view_rounded;
    bool layout_refresh;
    bool dialog_button_disabled;
    bool sort_on_cpu_by_default;
  };

  friend class TaskManagerViewTest;

  explicit TaskManagerView(StartAction start_action = StartAction::kOther);

  // Returns flags that describe how the TaskManagerView should be rendered.
  static TableConfigs GetTableConfigs();

  // Creates the header for the view.
  void CreateHeader(const ChromeLayoutProvider* provider);
  std::unique_ptr<views::View> CreateHeaderContent(
      const ChromeLayoutProvider* provider);
  std::unique_ptr<views::View> CreateHeaderSeparatorUnderlay(int height);

  // Creates a new TableModel which only operates on the subset of tasks
  // associated with the DisplayCategory (e.g. kTabs means only Tab processes
  // are displayed).
  void PerformFilter(DisplayCategory category);

  // Creates all corresponding subcomponents for the header.
  std::unique_ptr<views::TabbedPaneTabStrip> CreateTabbedPane(
      const ChromeLayoutProvider* provider,
      const gfx::Insets& title_insets,
      const gfx::Outsets& tab_outsets);
  std::unique_ptr<views::View> CreateSearchBar(
      const ChromeLayoutProvider* provider);
  std::unique_ptr<views::ScrollView> CreateProcessView(
      std::unique_ptr<views::TableView> tab_table,
      bool table_has_border,
      bool layout_refresh);

  // Creates the child controls (header, table, etc).
  void Init();

  // Initializes the state of the always-on-top setting as the window is shown.
  void InitAlwaysOnTopState();

  // Activates the tab associated with the selected row.
  void ActivateSelectedTab();

  // Selects the active tab in the specified browser window.
  void SelectTaskOfActiveTab(Browser* browser);

  // Restores saved "always on top" state from a previous session.
  void RetrieveSavedAlwaysOnTopState();

  // Restores saved tab.
  void RestoreSavedCategory();

  // Saves the provided category in the browser's local_state(). This is used to
  // restore the category on the next boot up of the Task Manager.
  void SaveCategoryToLocalState(DisplayCategory category);

  void EndSelectedProcess();
  void AnnounceTaskEnded(bool any_task_ended);
  bool IsEndProcessButtonEnabled() const;

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  std::unique_ptr<TaskManagerTableModel> table_model_;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // We need to own the text of the menu, the Windows API does not copy it.
  std::u16string always_on_top_menu_text_;

  raw_ptr<views::TableView, DanglingUntriaged> tab_table_;
  raw_ptr<views::View, DanglingUntriaged> tab_table_parent_;

  // Specifications on how to layout the table.
  TableConfigs table_config_;

  // all possible columns, not necessarily visible.
  std::vector<ui::TableColumn> columns_;

  // The tabs which holds different task categories which is not null if task
  // manager refresh is enabled.
  raw_ptr<views::TabbedPaneTabStrip> tabs_ = nullptr;

  // Search keyword the user input.
  std::u16string search_terms_;

  // This button is not the same as the dialog button. It is only non-null if
  // task manager refresh is enabled.
  raw_ptr<views::MdTextButton> end_process_btn_;

  // The first time this instance of the task manager was initialized.
  const base::TimeTicks start_time_ = base::TimeTicks::Now();

  // The last time a process was ended by the user.
  base::TimeTicks latest_end_process_time_ = base::TimeTicks::Now();

  // The number of times a process has been ended in this session.
  size_t end_process_count_ = 0;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;

  base::WeakPtrFactory<TaskManagerView> weak_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_VIEWS_TASK_MANAGER_VIEW_H_
