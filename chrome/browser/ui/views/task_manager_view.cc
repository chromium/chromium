// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/task_manager_view.h"

#include <stddef.h>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_manager/common/task_manager_features.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

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

const auto kTabDefinitions = std::to_array<TaskManagerView::FilterTab>(
    {{
         .title_id = IDS_TASK_MANAGER_CATEGORY_TABS_NAME,
         .associated_category = DisplayCategory::kTabs,
         .icon = &views::kNewTabIcon,
     },
     {
         .title_id = IDS_TASK_MANAGER_CATEGORY_EXTENSIONS_NAME,
         .associated_category = DisplayCategory::kExtensions,
         .icon = &vector_icons::kExtensionChromeRefreshIcon,
     },
     {
         .title_id = IDS_TASK_MANAGER_CATEGORY_SYSTEM_NAME,
         .associated_category = DisplayCategory::kSystem,
         .icon = &vector_icons::kSettingsOutlineIcon,
     }});

TaskManagerView::~TaskManagerView() {
  // Delete child views now, while our table model still exists.
  end_process_btn_ = nullptr;  // Destroyed by `right_aligned_container` below.
  RemoveAllChildViews();

  // When the view is destroyed, the lifecycle of the Task Manager is complete.
  task_manager::RecordCloseEvent(start_time_, base::TimeTicks::Now());
}

// static
task_manager::TaskManagerTableModel* TaskManagerView::Show(
    Browser* browser,
    StartAction start_action) {
  if (g_task_manager_view) {
    // If there's a Task manager window open already, just activate it.
    g_task_manager_view->SelectTaskOfActiveTab(browser);
    g_task_manager_view->GetWidget()->Activate();
    return g_task_manager_view->table_model_.get();
  }

  g_task_manager_view = new TaskManagerView(start_action);

  // On Chrome OS, pressing Search-Esc when there are no open browser windows
  // will open the task manager on the root window for new windows.
  gfx::NativeWindow context =
      browser ? browser->window()->GetNativeWindow() : nullptr;
  CreateDialogWidget(g_task_manager_view, context, nullptr);
  g_task_manager_view->GetDialogClientView()->SetBackgroundColor(
      kColorTaskManagerBackground);
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
  EndSelectedProcess();

  // Just kill the process, don't close the task manager dialog.
  return false;
}

bool TaskManagerView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return IsEndProcessButtonEnabled();
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
  if (end_process_btn_) {
    end_process_btn_->SetEnabled(IsEndProcessButtonEnabled());
  }
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
    ui::mojom::MenuSourceType source_type) {
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

TaskManagerView::TaskManagerView(StartAction start_action)
    : tab_table_(nullptr),
      tab_table_parent_(nullptr),
      is_always_on_top_(false) {
  task_manager::RecordNewOpenEvent(start_action);
  set_use_custom_frame(false);
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

// static
TaskManagerView::TableConfigs TaskManagerView::GetTableConfigs() {
  const bool tm_refresh_enabled =
      base::FeatureList::IsEnabled(features::kTaskManagerDesktopRefresh);
  return TableConfigs{
      .table_has_border = !tm_refresh_enabled,
      .header_padding = tm_refresh_enabled,
      .scroll_view_rounded = tm_refresh_enabled,
      .layout_refresh = tm_refresh_enabled,
      .dialog_button_disabled = tm_refresh_enabled,
  };
}

void TaskManagerView::PerformFilter(DisplayCategory category) {
  // Clear the old model.
  tab_table_->SetModel(nullptr);

  // Create and set the new model.
  table_model_ = std::make_unique<TaskManagerTableModel>(this, category);
  tab_table_->SetModel(table_model_.get());

  // Columns are already retrieved, however since the table model changed, the
  // refresh types for this model need to be set for each column. Otherwise, the
  // values for each column will stop updating.
  table_model_->RetrieveSavedColumnsSettingsAndUpdateTable();

  // Redraw the table immediately by scheduling a paint since the rows most
  // likely changed in between switching models.
  tab_table_->OnItemsChanged(/*start=*/0, table_model_->RowCount());
}

void TaskManagerView::TabSelectedAt(int index) {
  PerformFilter(kTabDefinitions[index].associated_category);
}

std::unique_ptr<views::View> TaskManagerView::CreateTabbedPane() {
  auto tabs = std::make_unique<views::TabbedPaneTabStrip>(
      views::TabbedPane::Orientation::kHorizontal,
      views::TabbedPane::TabStripStyle::kCompactWithIcon,
      /*tabbed_pane=*/nullptr);
  tabs->SetDefaultFlex(0);

  for (const auto& tab : kTabDefinitions) {
    tabs->AddTab(l10n_util::GetStringUTF16(tab.title_id), tab.icon);
  }
  tabs->set_listener(this);

  return tabs;
}

void TaskManagerView::CreateHeader(const ChromeLayoutProvider* provider) {
  // Header Parent
  auto header_layout = std::make_unique<views::BoxLayout>();
  header_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_layout->set_cross_axis_alignment(views::LayoutAlignment::kEnd);

  auto container = std::make_unique<views::View>();

  const int vertical_spacing = provider->GetDistanceMetric(
      DISTANCE_TASK_MANAGER_HEADER_VERTICAL_SPACING);
  const int horizontal_spacing = provider->GetDistanceMetric(
      DISTANCE_TASK_MANAGER_HEADER_HORIZONTAL_SPACING);
  const int separator_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);

  auto tabs = CreateTabbedPane();

  // Empty Container, Search Bar, End Task Button, and Separator
  auto empty_view = std::make_unique<views::View>();
  empty_view->SetProperty(views::kMarginsKey,
                          gfx::Insets::VH(0, horizontal_spacing));

  auto search_bar_container = CreateSearchBar(provider);

  auto end_process_btn = CreateEndProcessButton(
      gfx::Insets::TLBR(0, horizontal_spacing, vertical_spacing, 0));

  auto separator =
      CreateSeparator(gfx::Insets::TLBR(0, 0, separator_spacing, 0));
  // Allow empty spacing and the search bar to flex freely.
  header_layout->SetFlexForView(empty_view.get(), 2);
  header_layout->SetFlexForView(search_bar_container.get(), 3);

  // Set the layout manager for the parent container to BoxLayout.
  container->SetLayoutManager(std::move(header_layout));

  auto right_aligned_container = std::make_unique<views::View>();
  right_aligned_container->SetProperty(views::kMarginsKey,
                                       gfx::Insets::VH(0, horizontal_spacing));
  // The container holds search bar and end process button, so their layout
  // could keep consistent during resizing.
  auto right_aligned_container_layout = std::make_unique<views::BoxLayout>();
  right_aligned_container_layout->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  right_aligned_container_layout->set_cross_axis_alignment(
      views::LayoutAlignment::kCenter);
  right_aligned_container->SetLayoutManager(
      std::move(right_aligned_container_layout));

  right_aligned_container->AddChildView(std::move(search_bar_container));
  end_process_btn_ =
      right_aligned_container->AddChildView(std::move(end_process_btn));

  // Compose all parts into header.
  container->AddChildView(std::move(tabs));
  container->AddChildView(std::move(empty_view));
  container->AddChildView(std::move(right_aligned_container));

  // Attach header to the top of the dialog contents.
  AddChildView(std::move(container));

  // Attach separator below header.
  AddChildView(std::move(separator));
}

std::unique_ptr<views::View> TaskManagerView::CreateSearchBar(
    const ChromeLayoutProvider* provider) {
  const int vertical_spacing = provider->GetDistanceMetric(
      DISTANCE_TASK_MANAGER_HEADER_VERTICAL_SPACING);
  const int horizontal_spacing = provider->GetDistanceMetric(
      DISTANCE_TASK_MANAGER_HEADER_HORIZONTAL_SPACING);
  const int search_bar_container_radius = provider->GetCornerRadiusMetric(
      views::ShapeContextTokens::kOmniboxExpandedRadius);

  auto search_bar_layout = std::make_unique<views::BoxLayout>();
  search_bar_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  search_bar_layout->set_main_axis_alignment(views::LayoutAlignment::kEnd);
  search_bar_layout->set_cross_axis_alignment(views::LayoutAlignment::kEnd);

  auto search_bar_container = std::make_unique<views::View>();
  search_bar_container->SetBackground(views::CreateThemedRoundedRectBackground(
      kColorTaskManagerSearchBarBackground, search_bar_container_radius));
  search_bar_container->SetLayoutManager(std::move(search_bar_layout));
  search_bar_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, vertical_spacing, horizontal_spacing));

  auto search_bar = std::make_unique<TaskManagerSearchBarView>(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_SEARCH_PLACEHOLDER),
      gfx::Insets::VH(0, horizontal_spacing));
  search_bar_container->AddChildView(std::move(search_bar));

  return search_bar_container;
}

std::unique_ptr<views::MdTextButton> TaskManagerView::CreateEndProcessButton(
    const gfx::Insets& margins) {
  auto button = std::make_unique<views::MdTextButton>();
  button->SetText(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL_ACCESSIBILITY_NAME));
  button->SetStyle(ui::ButtonStyle::kProminent);
  button->SetProperty(views::kMarginsKey, margins);
  button->SetCallback(base::BindRepeating(&TaskManagerView::EndSelectedProcess,
                                          base::Unretained(this)));
  return button;
}

std::unique_ptr<views::Separator> TaskManagerView::CreateSeparator(
    const gfx::Insets& margins) {
  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(views::kMarginsKey, margins);
  return separator;
}

std::unique_ptr<views::ScrollView> TaskManagerView::CreateProcessView(
    std::unique_ptr<views::TableView> tab_table,
    bool table_has_border,
    bool layout_refresh) {
  auto scroll_view = views::TableView::CreateScrollViewWithTable(
      std::move(tab_table), table_has_border);

  if (layout_refresh) {
    scroll_view->SetLayoutManager(std::make_unique<views::FillLayout>());
    scroll_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
  }

  return scroll_view;
}

void TaskManagerView::Init() {
  const auto table_config = GetTableConfigs();

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
  table_model_ = std::make_unique<TaskManagerTableModel>(
      this, table_config.layout_refresh ? DisplayCategory::kTabs
                                        : DisplayCategory::kAll);
  tab_table->SetModel(table_model_.get());
  tab_table->SetGrouper(this);
  tab_table->SetSortOnPaint(true);
  tab_table->set_observer(this);
  tab_table->set_context_menu_controller(this);
  set_context_menu_controller(this);

  if (table_config.dialog_button_disabled) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  } else {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));
  }

  if (table_config.header_padding) {
    views::TableHeaderStyle header_style = {
        .cell_vertical_padding = 16,
        .cell_horizontal_padding = 12,
        .resize_bar_vertical_padding = 16,
        .separator_horizontal_padding = 6,
        .font_weight = gfx::Font::Weight::MEDIUM};
    tab_table->SetHeaderStyle(header_style);
  }

  const auto* provider = ChromeLayoutProvider::Get();

  // Margins around all contents
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

  // Setup Layout Manager for Dialog
  if (table_config.layout_refresh) {
    views::FlexLayout* content_layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    content_layout->SetOrientation(views::LayoutOrientation::kVertical);

    CreateHeader(provider);
  } else {
    SetUseDefaultFillLayout(true);
  }

  // Add Process List (a.k.a Scroll View)
  tab_table_parent_ = AddChildView(
      CreateProcessView(std::move(tab_table), table_config.table_has_border,
                        table_config.layout_refresh));

  if (table_config.scroll_view_rounded) {
    tab_table_parent_->SetPaintToLayer(ui::LAYER_TEXTURED);
    ui::Layer* scroll_view_layer = tab_table_parent_->layer();

    scroll_view_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(
        provider->GetCornerRadiusMetric(views::Emphasis::kHigh)));

    scroll_view_layer->SetIsFastRoundedCorner(true);
  }

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

void TaskManagerView::EndSelectedProcess() {
  using SelectedIndices = ui::ListSelectionModel::SelectedIndices;
  SelectedIndices selection(tab_table_->selection_model().selected_indices());
  for (int index : base::Reversed(selection)) {
    table_model_->KillTask(index);
  }
}

bool TaskManagerView::IsEndProcessButtonEnabled() const {
  const ui::ListSelectionModel::SelectedIndices& selections(
      tab_table_->selection_model().selected_indices());
  for (const auto& selection : selections) {
    if (!table_model_->IsTaskKillable(selection)) {
      return false;
    }
  }

  return !selections.empty() && TaskManagerInterface::IsEndProcessEnabled();
}

BEGIN_METADATA(TaskManagerView)
END_METADATA

}  // namespace task_manager

namespace chrome {

#if BUILDFLAG(IS_MAC)
// These are used by the Mac versions of |ShowTaskManager| and |HideTaskManager|
// if they decide to show the Views task manager instead of the Cocoa one.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(
    Browser* browser,
    task_manager::StartAction start_action) {
  return task_manager::TaskManagerView::Show(browser, start_action);
}

void HideTaskManagerViews() {
  task_manager::TaskManagerView::Hide();
}
#endif

}  // namespace chrome
