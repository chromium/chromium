// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/task_manager_view.h"

#include <stddef.h>

#include <string_view>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_manager/common/task_manager_features.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/accessibility/view_accessibility.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/grit/theme_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

const auto kTabDefinitions = std::to_array<TaskManagerView::FilterTab>({
    {
        .associated_category = DisplayCategory::kTabsAndExtensions,
        .title_id = IDS_TASK_MANAGER_CATEGORY_TABS_AND_EXTENSIONS_NAME,
        .icon = &kNewTabRefreshIcon,
    },
    {
        .associated_category = DisplayCategory::kSystem,
#if BUILDFLAG(IS_CHROMEOS)
        .title_id = IDS_TASK_MANAGER_CATEGORY_SYSTEM_NAME,
        .icon = &kLaptopIcon,
#else
        .title_id = IDS_TASK_MANAGER_CATEGORY_BROWSER_NAME,
        .icon = &kBrowserLogoIcon,
#endif
    },
    {
        .associated_category = DisplayCategory::kAll,
        .title_id = IDS_TASK_MANAGER_CATEGORY_ALL_NAME,
        .icon = &kViewListIcon,
    },
});

TaskManagerView::~TaskManagerView() {
  // Delete child views now, while our table model still exists.
  tabs_ = nullptr;  // Destroyed by `container` below.
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
      browser ? browser->window()->GetNativeWindow() : gfx::NativeWindow();
  CreateDialogWidget(g_task_manager_view, context, gfx::NativeView());
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

  if (g_task_manager_view->table_config_.layout_refresh &&
      ui::AXPlatform::GetInstance().IsScreenReaderActive()) {
    // For a11y: with the refreshed layout, the top-left most item should be
    // focused by default so screen readers read out the layout ltr (or flipped
    // for rtl).
    g_task_manager_view->tabs_->GetSelectedTab()->RequestFocus();
  }
#if BUILDFLAG(IS_CHROMEOS)
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
  if (g_task_manager_view) {
    g_task_manager_view->GetWidget()->Close();
  }
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
  if (!IsTableSorted()) {
    return TableSortDescriptor();
  }

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
#if BUILDFLAG(IS_CHROMEOS)
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
}

void TaskManagerView::OnDoubleClick() {
  ActivateSelectedTab();
}

void TaskManagerView::OnKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_RETURN) {
    ActivateSelectedTab();
  }
}

void TaskManagerView::OnWidgetInitialized() {
  GetOkButton()->GetViewAccessibility().SetDescription(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL_ACCESSIBILITY_NAME));
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

void TaskManagerView::SearchBarOnInputChanged(std::u16string_view query) {
  search_terms_ = query;
  PerformFilter(
      kTabDefinitions[tabs_->GetSelectedTabIndex()].associated_category);
}

TaskManagerView::TaskManagerView(StartAction start_action)
    : tab_table_(nullptr),
      tab_table_parent_(nullptr),
      table_config_(GetTableConfigs()),
      is_always_on_top_(false) {
  task_manager::RecordNewOpenEvent(start_action);
  set_use_custom_frame(false);
  SetHasWindowSizeControls(true);
#if !BUILDFLAG(IS_CHROMEOS)
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
      .header_style = tm_refresh_enabled,
      .table_refresh = tm_refresh_enabled,
      .scroll_view_rounded = tm_refresh_enabled,
      .layout_refresh = tm_refresh_enabled,
      .dialog_button_disabled = tm_refresh_enabled,
      .sort_on_cpu_by_default = tm_refresh_enabled,
  };
}

void TaskManagerView::TabSelectedAt(int index) {
  PerformFilter(kTabDefinitions[index].associated_category);
}

void TaskManagerView::CreateHeader(const ChromeLayoutProvider* provider) {
  const int separator_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);

  // The strategy to get the TabStrip Selector and the Separator to overlap
  // involves using a parent FillLayout (which will allow the child views to
  // overlap), and then attaching the content container (tabs, search bar, and
  // end process button), along with a separator container (where the separator
  // is locked at the bottom using a BoxLayout).
  auto parent_container =
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build();

  auto content_container = CreateHeaderContent(provider);
  auto separator_container = CreateHeaderSeparatorUnderlay(/*height=*/2);

  // Attach separator below header, and then contents over top.
  parent_container->AddChildView(std::move(separator_container));
  parent_container->AddChildView(std::move(content_container));

  // Add some spacing at the bottom between the header and the next view.
  parent_container->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(0, 0, separator_spacing, 0));

  // Attach header to the top of the dialog contents.
  AddChildView(std::move(parent_container));
}

std::unique_ptr<views::View> TaskManagerView::CreateHeaderContent(
    const ChromeLayoutProvider* provider) {
  // Header Parent
  auto header_layout = std::make_unique<views::BoxLayout>();
  header_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);

  auto container = std::make_unique<views::View>();

  auto tabs = CreateTabbedPane(
      provider,
      /*title_insets=*/
      gfx::Insets::TLBR(0, views::TabbedPaneTab::kDefaultTitleLeftMargin, 0, 0),
      /*tab_outsets=*/gfx::Outsets::VH(0, 16));

  // Empty Container, Search Bar
  auto empty_view = std::make_unique<views::View>();
  auto search_bar_container = CreateSearchBar(provider);

  // Allow empty spacing and the search bar to flex freely.
  header_layout->SetFlexForView(empty_view.get(), 7);
  header_layout->SetFlexForView(search_bar_container.get(), 3);

  // Set the layout manager for the content container to BoxLayout.
  container->SetLayoutManager(std::move(header_layout));

  // Compose all parts into header.
  tabs_ = container->AddChildView(std::move(tabs));
  container->AddChildView(std::move(empty_view));
  container->AddChildView(std::move(search_bar_container));

  return container;
}

std::unique_ptr<views::View> TaskManagerView::CreateHeaderSeparatorUnderlay(
    int height) {
  auto separator_container = std::make_unique<views::View>();
  auto separator_container_layout = std::make_unique<views::BoxLayout>();
  separator_container_layout->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  separator_container_layout->set_cross_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  auto separator =
      views::Builder<views::Separator>().SetPreferredLength(height).Build();
  separator_container_layout->SetFlexForView(separator.get(), 1);
  separator_container->SetLayoutManager(std::move(separator_container_layout));
  separator_container->AddChildView(std::move(separator));
  return separator_container;
}

void TaskManagerView::PerformFilter(DisplayCategory category) {
  SaveCategoryToLocalState(category);

  // When `select_on_remove_` is enabled, the selection will automatically jump
  // to some next/previous row if available. However, this setting needs to be
  // temporarily disabled during model updates to achieve the desired selection
  // changes. Specifically:
  // 1. When a tab is changed, some number of rows will get added and removed.
  // The intended behavior is to clear the selection between this tab switch.
  // 2. When the search term changes, if the selection should always be kept if
  // it's in the current list. If it's not (i.e. "Tab: unusual" is selected, and
  // the search term changes from "un" -> "unh"), then it should be removed, but
  // no other selection should be applied (a.k.a the selection should clear).

  tab_table_->SetSelectOnRemove(false);
  if (table_model_->UpdateModel(category, search_terms_)) {
    // Model row count may differ, leading to off-screen row rendering.
    // Recompute scroll position.
    tab_table_->InvalidateLayout();
  }
  tab_table_->SetSelectOnRemove(true);
}

std::unique_ptr<views::TabbedPaneTabStrip> TaskManagerView::CreateTabbedPane(
    const ChromeLayoutProvider* provider,
    const gfx::Insets& title_insets,
    const gfx::Outsets& tab_outsets) {
  const int tab_height =
      provider->GetDistanceMetric(DISTANCE_TASK_MANAGER_TAB_HEIGHT);
  const auto dialog_insets = provider->GetInsetsMetric(INSETS_TASK_MANAGER);

  auto tabs = std::make_unique<views::TabbedPaneTabStrip>(
      views::TabbedPane::Orientation::kHorizontal,
      views::TabbedPane::TabStripStyle::kWithIcon,
      /*tabbed_pane=*/nullptr);
  tabs->SetDefaultFlex(0);
  tabs->SetDrawTabDivider(false);

  for (const auto& tab_definition : kTabDefinitions) {
    auto* tab = tabs->AddTab(l10n_util::GetStringUTF16(tab_definition.title_id),
                             tab_definition.icon);
    tab->SetTitleMargin(title_insets);
    tab->SetTabOutsets(tab_outsets);

    // Assume some arbitrary spec_height. Set the height of the tabs as
    // (spec_height - task_manager_dialog_insets), so that the focus ring around
    // the tabbed pane isn't touching the title bar.
    tab->SetHeight(tab_height - dialog_insets.top());
  }
  tabs->set_listener(this);

  return tabs;
}

std::unique_ptr<views::View> TaskManagerView::CreateSearchBar(
    const ChromeLayoutProvider* provider) {
  const int horizontal_spacing = provider->GetDistanceMetric(
      DISTANCE_TASK_MANAGER_SEARCH_BAR_ICON_AND_BUTTON_HORIZONTAL_SPACING);
  const int search_bar_container_radius = provider->GetCornerRadiusMetric(
      views::ShapeContextTokens::kOmniboxExpandedRadius);

  auto search_bar_layout = std::make_unique<views::BoxLayout>();
  search_bar_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  search_bar_layout->set_cross_axis_alignment(views::LayoutAlignment::kStart);

  auto search_bar_container = std::make_unique<views::View>();
  search_bar_container->SetBackground(views::CreateRoundedRectBackground(
      kColorTaskManagerSearchBarBackground, search_bar_container_radius));
  search_bar_container->SetLayoutManager(std::move(search_bar_layout));
  const gfx::Size search_bar_size{
      provider->GetDistanceMetric(DISTANCE_TASK_MANAGER_SEARCH_BAR_MIN_WIDTH),
      provider->GetDistanceMetric(DISTANCE_TASK_MANAGER_SEARCH_BAR_MIN_HEIGHT)};
  search_bar_container->SetPreferredSize(search_bar_size);

  auto search_bar = std::make_unique<TaskManagerSearchBarView>(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_SEARCH_PLACEHOLDER),
      gfx::Insets::VH(0, horizontal_spacing), *this);
  search_bar_container->AddChildView(std::move(search_bar));

  return search_bar_container;
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
  // Create the table columns.
  for (size_t i = 0; i < kColumnsSize; ++i) {
    const auto& col_data = kColumns[i];
    columns_.emplace_back(col_data.id, col_data.align, col_data.width,
                          col_data.percent);
    columns_.back().sortable = col_data.sortable;
    columns_.back().initial_sort_is_ascending =
        col_data.initial_sort_is_ascending;
  }

  // Create the table view.
  auto tab_table = std::make_unique<views::TableView>(
      nullptr, columns_, views::TableType::kIconAndText, false);
  tab_table_ = tab_table.get();
  table_model_ = std::make_unique<TaskManagerTableModel>(
      this, table_config_.layout_refresh ? DisplayCategory::kTabsAndExtensions
                                         : DisplayCategory::kAll);
  tab_table->SetModel(table_model_.get());
  tab_table->SetGrouper(this);
  tab_table->SetGrouperVisibility(!table_config_.layout_refresh);
  tab_table->SetSortOnPaint(true);
  if (table_config_.layout_refresh) {
    // Disables alternating row colors on all platforms, including macOS.
    tab_table->SetAlternatingRowColorsEnabled(base::PassKey<TaskManagerView>(),
                                              false);
    tab_table->SetMouseHoveringEnabled(true);

    tab_table->SetRowPadding(views::DISTANCE_TABLE_VERTICAL_TEXT_PADDING);
  }
  tab_table->set_observer(this);
  tab_table->set_context_menu_controller(this);
  set_context_menu_controller(this);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(table_config_.layout_refresh
                                               ? IDS_TASK_MANAGER_KILL_V2
                                               : IDS_TASK_MANAGER_KILL));

  const auto* provider = ChromeLayoutProvider::Get();
  const float corner_radius =
      provider->GetCornerRadiusMetric(views::Emphasis::kHigh);

  if (table_config_.header_style) {
    views::TableHeaderStyle header_style(
        /*cell_vertical_padding=*/14, /*cell_horizontal_padding=*/12,
        /*resize_bar_vertical_padding=*/16,
        /*separator_horizontal_padding=*/0,
        /*font_weight=*/gfx::Font::Weight::MEDIUM,
        /*separator_horizontal_color_id=*/ui::kColorSysDivider,
        /*separator_vertical_color_id=*/ui::kColorSysDivider,
        /*background_color_id=*/kColorTaskManagerTableHeaderBackground,
        /*focus_ring_upper_corner_radius=*/corner_radius,
        /*header_sort_state=*/
        base::FeatureList::IsEnabled(features::kTaskManagerDesktopRefresh));
    tab_table->SetHeaderStyle(header_style);
  }

  if (table_config_.table_refresh) {
    views::TableStyle table_style = {
        .background_tokens =
            views::TableBackgroundStyle{
                .background = kColorTaskManagerTableBackground,
                .alternate = kColorTaskManagerTableBackgroundAlternate,
                .selected_focused =
                    kColorTaskManagerTableBackgroundSelectedFocused,
                .selected_unfocused =
                    kColorTaskManagerTableBackgroundSelectedUnfocused,
            },
        .icons_have_background = true,
        .inset_focus_ring = true,
    };
    tab_table->SetTableStyle(table_style);
  }

  // Margins around all contents
  const gfx::Insets dialog_insets = provider->GetInsetsMetric(
      table_config_.layout_refresh ? static_cast<int>(INSETS_TASK_MANAGER)
                                   : views::INSETS_DIALOG);
  // We don't use ChromeLayoutProvider::GetDialogInsetsForContentType because we
  // don't have a title.
  const auto content_insets = gfx::Insets::TLBR(
      dialog_insets.top(), dialog_insets.left(),
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
      dialog_insets.right());
  SetBorder(views::CreateEmptyBorder(content_insets));

  // Setup Layout Manager for Dialog
  if (table_config_.layout_refresh) {
    views::FlexLayout* content_layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    content_layout->SetOrientation(views::LayoutOrientation::kVertical);

    CreateHeader(provider);
  } else {
    SetUseDefaultFillLayout(true);
  }

  // Add Process List (a.k.a Scroll View)
  tab_table_parent_ = AddChildView(
      CreateProcessView(std::move(tab_table), table_config_.table_has_border,
                        table_config_.layout_refresh));

  if (table_config_.scroll_view_rounded) {
    tab_table_parent_->SetPaintToLayer(ui::LAYER_TEXTURED);

    ui::Layer* scroll_view_layer = tab_table_parent_->layer();
    scroll_view_layer->SetRoundedCornerRadius(
        gfx::RoundedCornersF(corner_radius));
    scroll_view_layer->SetIsFastRoundedCorner(true);
  }

  table_model_->RetrieveSavedColumnsSettingsAndUpdateTable(
      table_config_.sort_on_cpu_by_default);

  if (table_config_.layout_refresh) {
    RestoreSavedCategory();
  }

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
  if (active_row.has_value()) {
    table_model_->ActivateTask(active_row.value());
  }
}

void TaskManagerView::SelectTaskOfActiveTab(Browser* browser) {
  if (browser) {
    tab_table_->Select(table_model_->GetRowForWebContents(
        browser->tab_strip_model()->GetActiveWebContents()));
  }
}

void TaskManagerView::RetrieveSavedAlwaysOnTopState() {
  is_always_on_top_ = false;

  if (!g_browser_process->local_state()) {
    return;
  }

  const base::Value::Dict& dictionary =
      g_browser_process->local_state()->GetDict(GetWindowName());
  is_always_on_top_ = dictionary.FindBool("always_on_top").value_or(false);
}

void TaskManagerView::RestoreSavedCategory() {
  if (!g_browser_process->local_state()) {
    return;
  }

  int category =
      g_browser_process->local_state()->GetInteger(prefs::kTaskManagerCategory);
  int max = static_cast<int>(DisplayCategory::kMax);

  // Bounds check the retrieved pref.
  if (category < 0 || category > max) {
    category = static_cast<int>(TaskManagerTableModel::kDefaultCategory);
  }

  const auto parsed_category = static_cast<DisplayCategory>(category);

  // Finds the tab index of the category to restore, or does nothing if the
  // category no longer exists as a tab.
  for (size_t i = 0; i < kTabDefinitions.size(); ++i) {
    if (kTabDefinitions[i].associated_category == parsed_category) {
      tabs_->SelectTab(tabs_->GetTabAtIndex(i), /*animate=*/false);
      break;
    }
  }
}

void TaskManagerView::SaveCategoryToLocalState(DisplayCategory category) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    return;
  }

  // Stores the current tab to be restored again at startup.
  int category_to_save = static_cast<int>(category);
  int max = static_cast<int>(DisplayCategory::kMax);
  int default_category =
      static_cast<int>(TaskManagerTableModel::kDefaultCategory);

  // Bounds check to ensure that the category is set appropriately.
  if (category_to_save < 0 || category_to_save > max) {
    category_to_save = default_category;
  }
  local_state->SetInteger(prefs::kTaskManagerCategory, category_to_save);
}

void TaskManagerView::EndSelectedProcess() {
  using SelectedIndices = ui::ListSelectionModel::SelectedIndices;
  SelectedIndices selection(tab_table_->selection_model().selected_indices());
  bool any_task_ended = false;
  for (int index : base::Reversed(selection)) {
    any_task_ended |= table_model_->KillTask(index);
  }

  // AX: Announce the result of ending a task group.
  if (table_config_.layout_refresh) {
    AnnounceTaskEnded(any_task_ended);
  }

  base::TimeTicks current_time = base::TimeTicks::Now();
  if (end_process_count_ < 5) {
    task_manager::RecordEndProcessEvent(latest_end_process_time_, current_time,
                                        ++end_process_count_);
  }
  latest_end_process_time_ = current_time;
}

void TaskManagerView::AnnounceTaskEnded(bool any_task_ended) {
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      any_task_ended ? IDS_TASK_MANAGER_TASK_KILL_SUCCESS_ACCESSIBILITY_MESSAGE
                     : IDS_TASK_MANAGER_TASK_KILL_FAIL_ACCESSIBILITY_MESSAGE));
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
