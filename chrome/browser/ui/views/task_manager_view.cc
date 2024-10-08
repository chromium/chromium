// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/task_manager_view.h"

#include <stddef.h>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/grit/theme_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace task_manager {
namespace {

TaskManagerView* g_task_manager_view = nullptr;

}  // namespace

TaskManagerView::~TaskManagerView() {
  // Delete child views now, while our table model still exists.
  RemoveAllChildViews();
}

// static
task_manager::TaskManagerTableModel* TaskManagerView::Show(Browser* browser) {
  if (g_task_manager_view) {
    // If there's a Task manager window open already, just activate it.
    g_task_manager_view->SelectTaskOfActiveTab(browser);
    g_task_manager_view->GetWidget()->Activate();
    return g_task_manager_view->table_model_.get();
  }

  g_task_manager_view = new TaskManagerView();

  // On Chrome OS, pressing Search-Esc when there are no open browser windows
  // will open the task manager on the root window for new windows.
  gfx::NativeWindow context =
      browser ? browser->window()->GetNativeWindow() : nullptr;
  CreateDialogWidget(g_task_manager_view, context, nullptr);
  g_task_manager_view->InitAlwaysOnTopState();

#if BUILDFLAG(IS_WIN)
  // Set the app id for the task manager to the app id of its parent browser. If
  // no parent is specified, the app id will default to that of the initial
  // process.
  if (browser) {
    ui::win::SetAppIdForWindow(
        shell_integration::win::GetAppUserModelIdForBrowser(
            browser->profile()->GetPath()),
        views::HWNDForWidget(g_task_manager_view->GetWidget()));
  }
#endif

  g_task_manager_view->SelectTaskOfActiveTab(browser);
  g_task_manager_view->GetWidget()->Show();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  aura::Window* window = g_task_manager_view->GetWidget()->GetNativeWindow();
  // An app id for task manager windows, also used to identify the shelf item.
  // Generated as crx_file::id_util::GenerateId("org.chromium.taskmanager")
  static constexpr char kTaskManagerId[] = "ijaigheoohcacdnplfbdimmcfldnnhdi";
  const ash::ShelfID shelf_id(kTaskManagerId);
  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_DIALOG);
  window->SetTitle(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE));
#endif
  return g_task_manager_view->table_model_.get();
}

// static
void TaskManagerView::Hide() {
  if (g_task_manager_view)
    g_task_manager_view->GetWidget()->Close();
}

bool TaskManagerView::IsColumnVisible(int column_id) const {
  return tab_table_->IsColumnVisible(column_id);
}

bool TaskManagerView::SetColumnVisibility(int column_id, bool new_visibility) {
  // Check if there is at least 1 visible column before changing the visibility.
  // If this column would be the last column to be visible and its hiding, then
  // prevent this column visibility change. see crbug.com/1320307 for details.
  if (!new_visibility && tab_table_->visible_columns().size() <= 1) {
    return false;
  }

  const bool currently_visible = tab_table_->IsColumnVisible(column_id);
  tab_table_->SetColumnVisibility(column_id, new_visibility);
  return new_visibility != currently_visible;
}

bool TaskManagerView::IsTableSorted() const {
  return tab_table_->GetIsSorted();
}

TableSortDescriptor TaskManagerView::GetSortDescriptor() const {
  if (!IsTableSorted())
    return TableSortDescriptor();

  const auto& descriptor = tab_table_->sort_descriptors().front();
  return TableSortDescriptor(descriptor.column_id, descriptor.ascending);
}

void TaskManagerView::SetSortDescriptor(const TableSortDescriptor& descriptor) {
  views::TableView::SortDescriptors descriptor_list;

  // If |sorted_column_id| is the default value, it means to clear the sort.
  if (descriptor.sorted_column_id != TableSortDescriptor().sorted_column_id) {
    descriptor_list.emplace_back(descriptor.sorted_column_id,
                                 descriptor.is_ascending);
  }

  tab_table_->SetSortDescriptors(descriptor_list);
}

void TaskManagerView::MaybeHighlightActiveTask() {
  if (table_model_ && tab_table_->selection_model().empty()) {
    std::optional<size_t> row = table_model_->GetRowForActiveTask();
    if (row.has_value()) {
      tab_table_->Select(row.value());
    }
  }
}

gfx::Size TaskManagerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // The TaskManagerView's preferred size is used to size the hosting Widget
  // when the Widget does not have `initial_restored_bounds_` set. The minimum
  // width below ensures that there is sufficient space for the task manager's
  // columns when the above restored bounds have not been set.
  return gfx::Size(640, 270);
}

bool TaskManagerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const bool is_valid_modifier =
      accelerator.modifiers() == ui::EF_CONTROL_DOWN ||
      accelerator.modifiers() == (ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  DCHECK(is_valid_modifier);
  DCHECK_EQ(ui::VKEY_W, accelerator.key_code());

  GetWidget()->Close();
  return true;
}

views::View* TaskManagerView::GetInitiallyFocusedView() {
  // Set initial focus to |table_view_| so that screen readers can navigate the
  // UI when the dialog is opened without having to manually assign focus first.
  return tab_table_;
}

bool TaskManagerView::ExecuteWindowsCommand(int command_id) {
  return false;
}

ui::ImageModel TaskManagerView::GetWindowIcon() {
  TRACE_EVENT0("ui", "TaskManagerView::GetWindowIcon");
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40739545): Move apps::CreateStandardIconImage to some
  // where lower in the stack.
  return ui::ImageModel::FromImageSkia(apps::CreateStandardIconImage(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_ASH_SHELF_ICON_TASK_MANAGER)));
#else
  return views::DialogDelegateView::GetWindowIcon();
#endif
}

std::string TaskManagerView::GetWindowName() const {
  return prefs::kTaskManagerWindowPlacement;
}

bool TaskManagerView::Accept() {
  using SelectedIndices = ui::ListSelectionModel::SelectedIndices;
  SelectedIndices selection(tab_table_->selection_model().selected_indices());
  for (int index : base::Reversed(selection)) {
    table_model_->KillTask(index);
  }

  // Just kill the process, don't close the task manager dialog.
  return false;
}

bool TaskManagerView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  const ui::ListSelectionModel::SelectedIndices& selections(
      tab_table_->selection_model().selected_indices());
  for (const auto& selection : selections) {
    if (!table_model_->IsTaskKillable(selection))
      return false;
  }

  return !selections.empty() && TaskManagerInterface::IsEndProcessEnabled();
}

void TaskManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_task_manager_view == this) {
    // We don't have to delete |g_task_manager_view| as we don't own it. It's
    // owned by the Views hierarchy.
    g_task_manager_view = nullptr;
  }
  table_model_->StoreColumnsSettings();
}

void TaskManagerView::GetGroupRange(size_t model_index,
                                    views::GroupRange* range) {
  table_model_->GetRowsGroupRange(model_index, &range->start, &range->length);
}

void TaskManagerView::OnSelectionChanged() {
  DialogModelChanged();
}

void TaskManagerView::OnDoubleClick() {
  ActivateSelectedTab();
}

void TaskManagerView::OnKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_RETURN)
    ActivateSelectedTab();
}

void TaskManagerView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  for (const auto& table_column : columns_) {
    menu_model_->AddCheckItem(table_column.id,
                              l10n_util::GetStringUTF16(table_column.id));
  }

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);

  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

bool TaskManagerView::IsCommandIdChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

bool TaskManagerView::IsCommandIdEnabled(int id) const {
  return true;
}

void TaskManagerView::ExecuteCommand(int id, int event_flags) {
  table_model_->ToggleColumnVisibility(id);
}

void TaskManagerView::MenuClosed(ui::SimpleMenuModel* source) {
  menu_model_.reset();
  menu_runner_.reset();
}

TaskManagerView::TaskManagerView()
    : tab_table_(nullptr),
      tab_table_parent_(nullptr),
      is_always_on_top_(false) {
  set_use_custom_frame(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));
  SetHasWindowSizeControls(true);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the widget's frame should not show the window title.
  SetTitle(IDS_TASK_MANAGER_TITLE);
#endif

  // Avoid calling Accept() when closing the dialog, since Accept() here means
  // "kill task" (!).
  // TODO(ellyjones): Remove this once the Accept() override is removed from
  // this class.
  SetCloseCallback(base::DoNothing());

  Init();
}

// static
TaskManagerView* TaskManagerView::GetInstanceForTests() {
  return g_task_manager_view;
}

void TaskManagerView::Init() {
  // Create the table columns.
  for (size_t i = 0; i < kColumnsSize; ++i) {
    const auto& col_data = kColumns[i];
    columns_.push_back(ui::TableColumn(col_data.id, col_data.align,
                                       col_data.width, col_data.percent));
    columns_.back().sortable = col_data.sortable;
    columns_.back().initial_sort_is_ascending =
        col_data.initial_sort_is_ascending;
  }

  // Create the table view.
  auto tab_table = std::make_unique<views::TableView>(
      nullptr, columns_, views::TableType::kIconAndText, false);
  tab_table_ = tab_table.get();
  table_model_ = std::make_unique<TaskManagerTableModel>(this);
  tab_table->SetModel(table_model_.get());
  tab_table->SetGrouper(this);
  tab_table->SetSortOnPaint(true);
  tab_table->set_observer(this);
  tab_table->set_context_menu_controller(this);
  set_context_menu_controller(this);

  const auto* provider = ChromeLayoutProvider::Get();
  const bool tm_refresh_enabled =
      base::FeatureList::IsEnabled(features::kTaskManagerDesktopRefresh);

  // Has a border if the feature is disabled, since the redesign version doesn't
  // have a border.
  bool table_has_border = !tm_refresh_enabled,
       large_header_padding = tm_refresh_enabled,
       scroll_view_rounded = tm_refresh_enabled;

  if (large_header_padding) {
    views::TableHeaderStyle header_style = {/*vertical_padding=*/16,
                                            /*horizontal_padding=*/8};
    tab_table->SetHeaderStyle(header_style);
  }

  tab_table_parent_ = AddChildView(views::TableView::CreateScrollViewWithTable(
      std::move(tab_table), table_has_border));

  if (scroll_view_rounded) {
    tab_table_parent_->SetPaintToLayer(ui::LAYER_TEXTURED);
    ui::Layer* scroll_view_layer = tab_table_parent_->layer();

    scroll_view_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(
        provider->GetCornerRadiusMetric(views::Emphasis::kHigh)));

    scroll_view_layer->SetIsFastRoundedCorner(true);
  }

  SetUseDefaultFillLayout(true);

  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  // We don't use ChromeLayoutProvider::GetDialogInsetsForContentType because we
  // don't have a title.
  const auto content_insets = gfx::Insets::TLBR(
      dialog_insets.top(), dialog_insets.left(),
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
      dialog_insets.right());
  SetBorder(views::CreateEmptyBorder(content_insets));

  table_model_->RetrieveSavedColumnsSettingsAndUpdateTable();

  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
  AddAccelerator(
      ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
}

void TaskManagerView::InitAlwaysOnTopState() {
  RetrieveSavedAlwaysOnTopState();
  GetWidget()->SetZOrderLevel(is_always_on_top_
                                  ? ui::ZOrderLevel::kFloatingWindow
                                  : ui::ZOrderLevel::kNormal);
}

void TaskManagerView::ActivateSelectedTab() {
  const std::optional<size_t> active_row =
      tab_table_->selection_model().active();
  if (active_row.has_value())
    table_model_->ActivateTask(active_row.value());
}

void TaskManagerView::SelectTaskOfActiveTab(Browser* browser) {
  if (browser) {
    tab_table_->Select(table_model_->GetRowForWebContents(
        browser->tab_strip_model()->GetActiveWebContents()));
  }
}

void TaskManagerView::RetrieveSavedAlwaysOnTopState() {
  is_always_on_top_ = false;

  if (!g_browser_process->local_state())
    return;

  const base::Value::Dict& dictionary =
      g_browser_process->local_state()->GetDict(GetWindowName());
  is_always_on_top_ = dictionary.FindBool("always_on_top").value_or(false);
}

BEGIN_METADATA(TaskManagerView)
END_METADATA

}  // namespace task_manager

namespace chrome {

#if BUILDFLAG(IS_MAC)
// These are used by the Mac versions of |ShowTaskManager| and |HideTaskManager|
// if they decide to show the Views task manager instead of the Cocoa one.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser) {
  return task_manager::TaskManagerView::Show(browser);
}

void HideTaskManagerViews() {
  task_manager::TaskManagerView::Hide();
}
#endif

}  // namespace chrome
