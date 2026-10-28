// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_block_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_chip_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_item_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_minor_text_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_controller.h"
#include "chrome/browser/ui/views/app_menu/app_menu_zoom_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ax_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

bool ShouldRoundBottomCorners(size_t index,
                              const actions::ActionListVector& items) {
  // An item rounds its bottom corners if it is the last non-divider item in
  // its list, if it is the last notification item in a contiguous notification
  // group, OR if it is or precedes the zoom submenu.
  actions::BaseAction* const base_item = items[index].get();
  if (base_item->GetActionItem()->GetActionId() == kActionZoomSubmenu) {
    return true;
  }
  if (base_item->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
      AppMenuActionItem::DisplayType::kNotification) {
    // Adjacent notification items without a spacing separator between them
    // (such as "Relaunch to update" and "Relaunch later...") share a single
    // rounded container, so only the last visible item in the contiguous group
    // rounds its bottom corners.
    for (size_t i = index + 1; i < items.size(); ++i) {
      if (items[i]->GetActionItem()->GetVisible()) {
        return items[i]->GetProperty(AppMenuActionItem::kDisplayTypeKey) !=
               AppMenuActionItem::DisplayType::kNotification;
      }
    }
    return true;
  }
  for (size_t i = index + 1; i < items.size(); ++i) {
    if (!items[i]->GetActionItem()->GetVisible()) {
      continue;
    }
    const auto display_type =
        items[i]->GetProperty(AppMenuActionItem::kDisplayTypeKey);
    if (display_type != AppMenuActionItem::DisplayType::kDivider &&
        display_type != AppMenuActionItem::DisplayType::kHeader) {
      return items[i]->GetActionItem()->GetActionId() == kActionZoomSubmenu;
    }
  }
  return true;
}

bool ShouldRoundTopCorners(size_t index,
                           const actions::ActionListVector& items) {
  // An item rounds its top corners if it is the first item or if the
  // preceding non-divider item rounded its bottom corners.
  for (size_t i = index; i > 0; --i) {
    size_t prev_index = i - 1;
    if (!items[prev_index]->GetActionItem()->GetVisible()) {
      continue;
    }
    const auto display_type =
        items[prev_index]->GetProperty(AppMenuActionItem::kDisplayTypeKey);
    if (display_type == AppMenuActionItem::DisplayType::kDivider ||
        display_type == AppMenuActionItem::DisplayType::kHeader) {
      continue;
    }
    return ShouldRoundBottomCorners(prev_index, items);
  }
  return true;
}

bool IsAdjacentToDivider(size_t index,
                         const actions::ActionListVector& items,
                         bool preceding) {
  const int step = preceding ? -1 : 1;
  const int start = static_cast<int>(index) + step;
  const int end = preceding ? -1 : static_cast<int>(items.size());
  for (int i = start; i != end; i += step) {
    if (!items[i]->GetActionItem()->GetVisible()) {
      continue;
    }
    const auto display_type =
        items[i]->GetProperty(AppMenuActionItem::kDisplayTypeKey);
    return display_type == AppMenuActionItem::DisplayType::kDivider &&
           items[i]->GetProperty(AppMenuActionItem::kSeparatorKey) !=
               ui::MenuSeparatorType::SPACING_SEPARATOR;
  }
  return false;
}

int GetCornerRadius(size_t index,
                    const actions::ActionListVector& items,
                    bool is_top) {
  const auto* provider = ChromeLayoutProvider::Get();
  if (is_top ? ShouldRoundTopCorners(index, items)
             : ShouldRoundBottomCorners(index, items)) {
    return provider->GetCornerRadiusMetric(kActionAppMenuContainerCornerRadius);
  }
  if (IsAdjacentToDivider(index, items, /*preceding=*/is_top)) {
    return provider->GetCornerRadiusMetric(
        kActionAppMenuDividerAdjacentCornerRadius);
  }
  return 0;
}

bool SupportsVerticalPadding(const actions::BaseAction* item) {
  return item->GetProperty(AppMenuActionItem::kItemHeightKey) ==
         AppMenuActionItem::ItemHeight::kCompact;
}

bool ShouldAddTopPadding(size_t index, const actions::ActionListVector& items) {
  if (!SupportsVerticalPadding(items[index].get())) {
    return false;
  }
  for (size_t i = index; i > 0; --i) {
    actions::BaseAction* const prev_base = items[i - 1].get();
    const auto display_type =
        prev_base->GetProperty(AppMenuActionItem::kDisplayTypeKey);
    if (!prev_base->GetActionItem()->GetVisible() ||
        display_type == AppMenuActionItem::DisplayType::kDivider ||
        display_type == AppMenuActionItem::DisplayType::kHeader) {
      continue;
    }
    return !SupportsVerticalPadding(prev_base);
  }
  return true;
}

bool ShouldAddBottomPadding(size_t index,
                            const actions::ActionListVector& items) {
  return SupportsVerticalPadding(items[index].get()) &&
         ShouldRoundBottomCorners(index, items);
}

actions::ActionInvocationContext BuildActionInvocationContext(
    const actions::BaseAction* base_action,
    int mouse_event_flags) {
  return actions::ActionInvocationContext::Builder()
      .SetProperty(chrome::kDispositionKey,
                   ui::DispositionFromEventFlags(mouse_event_flags))
      .SetProperty(chrome::kActionInvocationSourceKey,
                   chrome::ActionInvocationSource::kAppMenu)
      .SetProperty(AppMenuActionItem::kActionParamKey,
                   base_action->GetProperty(AppMenuActionItem::kActionParamKey))
      .SetProperty(kSidePanelOpenTriggerKey,
                   static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                       SidePanelOpenTrigger::kAppMenu))
      .Build();
}

}  // namespace

ActionAppMenu::ActionAppMenu(BrowserWindowInterface* browser_window_interface,
                             base::RepeatingClosure on_menu_closed_callback)
    : browser_window_interface_(browser_window_interface),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)),
      menu_manager_(
          std::make_unique<ActionAppMenuManager>(browser_window_interface,
                                                 /*drag_and_drop_host=*/this)) {
  menu_manager_->CreateMenuHierarchy();
}

ActionAppMenu::~ActionAppMenu() {
  search_bar_ = nullptr;
  command_to_action_map_.clear();
  search_controller_.reset();
  menu_manager_.reset();
}

void ActionAppMenu::RunMenu(views::MenuButtonController* host, int run_types) {
  RunMenu(host->button()->GetWidget(), host,
          host->button()->GetAnchorBoundsInScreen(), run_types,
          ui::mojom::MenuSourceType::kNone);
}

void ActionAppMenu::RunMenu(views::Widget* parent,
                            const gfx::Rect& anchor_screen_bounds,
                            int run_types,
                            ui::mojom::MenuSourceType source_type) {
  RunMenu(parent, nullptr, anchor_screen_bounds, run_types, source_type);
}

void ActionAppMenu::RunMenu(views::Widget* parent,
                            views::MenuButtonController* host,
                            const gfx::Rect& anchor_screen_bounds,
                            int run_types,
                            ui::mojom::MenuSourceType source_type) {
  auto root = std::make_unique<views::MenuItemView>(/*delegate=*/this);
  // Stash the raw pointer before transferring the unique_ptr ownership to
  // `menu_runner_`. This allows us to reference the root menu item view later.
  root_ = root.get();

  const auto* provider = ChromeLayoutProvider::Get();
  root_->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM)));
  root_->set_children_use_full_width(true);

  root_->CreateSubmenu();
  PopulateMenu(root_, menu_manager_->GetAppMenuRoot());
  root_->GetSubmenu()->set_use_scroll_bar(true);

  // Temporary code to test search functionality while the key event routing
  // design is aligned on. Prefills the search bar with the query from the
  // `kChroMenuSearchQuery` feature param.
  if (search_bar_) {
    const std::string query = features::kChroMenuSearchQuery.Get();
    if (!query.empty()) {
      search_bar_->SetText(base::UTF8ToUTF16(query));
    }
  }

  int32_t types = views::MenuRunner::HAS_MNEMONICS;
  // Forward the keyboard-related flags, so that whenever the menu is opened
  // with a key press it shows mnemonic underlines. This also selects the first
  // item (on Windows only).
  types |= run_types & (views::MenuRunner::SHOULD_SHOW_MNEMONICS |
                        views::MenuRunner::INVOKED_FROM_KEYBOARD);
  menu_runner_ = std::make_unique<views::MenuRunner>(std::move(root), types);

  metrics_.OnMenuOpened();

  menu_runner_->RunMenuAt(parent, host, anchor_screen_bounds,
                          views::MenuAnchorPosition::kTopRight, source_type,
                          /*native_view_for_gestures=*/gfx::NativeView(),
                          /*corners=*/std::nullopt,
                          "Chrome.AppMenu.MenuHostInitToNextFramePresented");

  auto* screen = display::Screen::Get();
  CHECK(screen);
  int screen_height =
      screen->GetDisplayNearestPoint(anchor_screen_bounds.origin())
          .bounds()
          .height();

  int menu_height = 0;
  if (IsShowing()) {
    auto* const scroll_view_container =
        root_->GetSubmenu()->GetScrollViewContainer();
    if (scroll_view_container) {
      menu_height = scroll_view_container->GetPreferredSize({}).height();
    } else {
      menu_height = root_->GetSubmenu()->GetPreferredSize({}).height();
    }
    metrics_.RecordMenuHeightPercentage(menu_height, screen_height);
  }
}

void ActionAppMenu::UpdateMenuItem(actions::BaseAction* action,
                                   actions::BaseAction* target_parent_action,
                                   actions::BaseAction* insert_after) {
  CHECK(action);
  CHECK_NE(action, insert_after);
  actions::BaseAction* old_parent_action = action->GetParent();
  CHECK(old_parent_action);

  views::MenuItemView* moved_item = GetMenuItemForAction(action);
  std::unique_ptr<actions::BaseAction> owned_action =
      old_parent_action->RemoveChild(action);

  size_t target_index = 0;
  if (target_parent_action) {
    const auto& target_children =
        target_parent_action->GetChildren().children();
    if (insert_after) {
      auto it = std::ranges::find_if(
          target_children,
          [insert_after](const std::unique_ptr<actions::BaseAction>& child) {
            return child.get() == insert_after;
          });
      CHECK(it != target_children.end());
      target_index = std::distance(target_children.begin(), it) + 1;
    }
    target_parent_action->AddChildAt(std::move(owned_action), target_index);
  }

  views::MenuItemView* old_parent_menu =
      moved_item ? moved_item->GetParentMenuItem() : nullptr;
  if (old_parent_menu) {
    RemoveSubmenuActionsFromMap(moved_item);
    command_to_action_map_.erase(moved_item->GetCommand());
    old_parent_menu->RemoveMenuItem(moved_item);
  }

  if (!target_parent_action) {
    if (old_parent_menu) {
      old_parent_menu->ChildrenChanged();
    }
    return;
  }

  views::MenuItemView* prev_item =
      insert_after ? GetMenuItemForAction(insert_after) : nullptr;
  views::MenuItemView* new_parent_menu =
      prev_item ? prev_item->GetParentMenuItem()
                : GetMenuItemForAction(target_parent_action);
  if (!new_parent_menu || !new_parent_menu->HasSubmenu() ||
      (target_parent_action->HasPopulateChildActionsCallback() &&
       new_parent_menu->GetSubmenu()->GetMenuItems().empty())) {
    if (old_parent_menu) {
      old_parent_menu->ChildrenChanged();
    }
    return;
  }

  // Indices are relative to the scrollable region (the SubmenuView's children).
  // If `prev_item` is pinned in the header, insert at the top of that region.
  const std::optional<size_t> prev_index =
      prev_item ? new_parent_menu->GetSubmenu()->GetIndexOf(prev_item)
                : std::nullopt;
  const size_t view_index = prev_index ? *prev_index + 1 : 0;

  const auto& target_children = target_parent_action->GetChildren().children();
  views::MenuItemView* new_item =
      AppendMenuItem(action, new_parent_menu, view_index);
  ConfigureMenuItem(new_item, action,
                    GetCornerRadius(target_index, target_children,
                                    /*is_top=*/true),
                    GetCornerRadius(target_index, target_children,
                                    /*is_top=*/false),
                    ShouldAddTopPadding(target_index, target_children),
                    ShouldAddBottomPadding(target_index, target_children));
  if (!action->HasPopulateChildActionsCallback()) {
    PopulateMenu(new_item, action);
  }

  if (old_parent_menu && old_parent_menu != new_parent_menu) {
    old_parent_menu->ChildrenChanged();
  }
  new_parent_menu->ChildrenChanged();
}

void ActionAppMenu::CloseMenu() {
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
}

bool ActionAppMenu::IsShowing() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ActionAppMenu::ExecuteCommand(int id, int mouse_event_flags) {
  auto action_iterator = command_to_action_map_.find(id);
  CHECK(action_iterator != command_to_action_map_.end());

  actions::BaseAction* base_action = action_iterator->second;
  if (base_action->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
      AppMenuActionItem::DisplayType::kCustom) {
    // Custom rows are represented by child views and do not have a row-level
    // action to execute.
    return;
  }

  actions::ActionItem* action_ptr = base_action->GetActionItem();
  CHECK(action_ptr);

  metrics_.LogMenuAction(base_action);

  action_ptr->InvokeAction(
      BuildActionInvocationContext(base_action, mouse_event_flags));
}

bool ActionAppMenu::ShouldExecuteCommandWithoutClosingMenu(int id,
                                                           const ui::Event& e) {
  // Zoom in/out are repeatable; keep the menu open so search result rows
  // behave like the inline AppMenuZoomView buttons.
  auto action_iterator = command_to_action_map_.find(id);
  if (action_iterator == command_to_action_map_.end()) {
    return false;
  }
  const std::optional<actions::ActionId> action_id =
      action_iterator->second->GetActionItem()->GetActionId();
  return action_id == kActionZoomMinus || action_id == kActionZoomPlus;
}

void ActionAppMenu::OnMenuClosed(views::MenuItemView* menu) {
  actions::ActionItem* action_to_execute = nullptr;
  actions::ActionInvocationContext action_context;
  if (action_to_execute_on_close_) {
    auto action_iterator =
        command_to_action_map_.find(action_to_execute_on_close_->action_id);
    CHECK(action_iterator != command_to_action_map_.end());
    action_to_execute = action_iterator->second->GetActionItem();
    action_context = std::move(action_to_execute_on_close_->context);
  }

  search_bar_ = nullptr;
  has_notification_header_ = false;
  action_to_execute_on_close_.reset();
  command_to_action_map_.clear();
  section_header_count_ = 0;
  if (on_menu_closed_callback_) {
    on_menu_closed_callback_.Run();
  }

  // menu_manager_->OnMenuClosed() deletes the menu's ActionItems. The
  // action yet to be executed and the search results can point to those
  // ActionItems, so execute the action first and reset search_controller
  // to avoid dangling pointers.
  if (action_to_execute) {
    action_to_execute->InvokeAction(std::move(action_context));
  }
  search_controller_.reset();

  menu_manager_->OnMenuClosed();
}

void ActionAppMenu::WillShowMenu(views::MenuItemView* menu) {
  if (!menu->HasSubmenu()) {
    return;
  }

  metrics_.OnWillShowSubMenu(menu->GetCommand());

  if (!menu->GetSubmenu()->GetMenuItems().empty()) {
    return;
  }

  auto action_iterator = command_to_action_map_.find(menu->GetCommand());
  if (action_iterator != command_to_action_map_.end() &&
      action_iterator->second->HasPopulateChildActionsCallback()) {
    PopulateMenu(menu, action_iterator->second);
  }
}

bool ActionAppMenu::IsItemChecked(int id) const {
  auto action_iterator = command_to_action_map_.find(id);
  if (action_iterator == command_to_action_map_.end()) {
    return false;
  }

  actions::ActionItem* action_ptr = action_iterator->second->GetActionItem();
  CHECK(action_ptr);
  return action_ptr->GetChecked();
}

const gfx::FontList* ActionAppMenu::GetLabelFontList(int id) const {
  auto action_iterator = command_to_action_map_.find(id);
  if (action_iterator == command_to_action_map_.end()) {
    return nullptr;
  }

  actions::BaseAction* base_action = action_iterator->second;
  CHECK(base_action);
  if (base_action->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
      AppMenuActionItem::DisplayType::kHeader) {
    return &views::TypographyProvider::Get().GetFont(
        views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5);
  } else if (base_action->GetActionItem()->GetActionId() ==
             kActionProfileSubmenu) {
    return &views::TypographyProvider::Get().GetFont(
        views::style::CONTEXT_MENU, views::style::STYLE_BODY_3_MEDIUM);
  }
  return &views::TypographyProvider::Get().GetFont(views::style::CONTEXT_LABEL,
                                                   views::style::STYLE_BODY_4);
}

std::optional<SkColor> ActionAppMenu::GetLabelColor(int id) const {
  auto action_iterator = command_to_action_map_.find(id);
  if (action_iterator == command_to_action_map_.end() || !root_ ||
      !root_->GetSubmenu()) {
    return std::nullopt;
  }

  const ui::ColorProvider* color_provider =
      root_->GetSubmenu()->GetColorProvider();
  if (!color_provider) {
    return std::nullopt;
  }

  actions::BaseAction* base_action = action_iterator->second;
  if (base_action->GetProperty(AppMenuActionItem::kDisplayTypeKey) !=
          AppMenuActionItem::DisplayType::kHeader &&
      !base_action->GetActionItem()->GetEnabled()) {
    return std::nullopt;
  }

  return color_provider->GetColor(
      base_action->GetProperty(AppMenuActionItem::kForegroundColorKey));
}

int ActionAppMenu::GetMaxWidthForMenu(views::MenuItemView* menu) {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_MAX_WIDTH);
}

bool ActionAppMenu::GetDropFormats(
    views::MenuItemView* menu,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  actions::BaseAction* action = GetActionForMenuItem(menu);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    return delegate->GetDropFormats(action, formats, format_types);
  }
  return false;
}

bool ActionAppMenu::AreDropTypesRequired(views::MenuItemView* menu) {
  actions::BaseAction* action = GetActionForMenuItem(menu);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    return delegate->AreDropTypesRequired(action);
  }
  return false;
}

bool ActionAppMenu::CanDrop(views::MenuItemView* menu,
                            const ui::OSExchangeData& data) {
  actions::BaseAction* action = GetActionForMenuItem(menu);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    return delegate->CanDrop(action, data);
  }
  return false;
}

ui::mojom::DragOperation ActionAppMenu::GetDropOperation(
    views::MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  actions::BaseAction* action = GetActionForMenuItem(item);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    return delegate->GetDropOperation(action, event, position);
  }
  return ui::mojom::DragOperation::kNone;
}

views::View::DropCallback ActionAppMenu::GetDropCallback(
    views::MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  actions::BaseAction* action = GetActionForMenuItem(menu);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    return delegate->GetDropCallback(action, position, event);
  }
  return base::DoNothing();
}

bool ActionAppMenu::CanDrag(views::MenuItemView* menu) {
  actions::BaseAction* action = GetActionForMenuItem(menu);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    return delegate->CanDrag(action);
  }
  return false;
}

void ActionAppMenu::WriteDragData(views::MenuItemView* sender,
                                  ui::OSExchangeData* data) {
  actions::BaseAction* action = GetActionForMenuItem(sender);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    delegate->WriteDragData(action, data);
  }
}

int ActionAppMenu::GetDragOperations(views::MenuItemView* sender) {
  actions::BaseAction* action = GetActionForMenuItem(sender);
  if (AppMenuDragAndDropDelegate* delegate = GetDragAndDropDelegate(action)) {
    return delegate->GetDragOperations(action);
  }
  return MenuDelegate::GetDragOperations(sender);
}

bool ActionAppMenu::ShouldCloseOnDragDropCompleted() {
  return false;
}

void ActionAppMenu::ShowSearchResults(actions::ActionItem* results) {
  ClearItemsBelowSearchBar();
  PopulateMenu(root_, results);
  root_->ChildrenChanged();
}

void ActionAppMenu::RestoreMenu() {
  ClearItemsBelowSearchBar();
  PopulateItemsBelowSearchBar();
  root_->ChildrenChanged();
}

bool ActionAppMenu::ShouldDragActivateChildButtons() const {
  return true;
}

void ActionAppMenu::SetTimerForTesting(base::ElapsedTimer timer) {
  menu_manager_->SetTimerForTesting(timer);       // IN-TEST
  metrics_.SetTimerForTesting(std::move(timer));  // IN-TEST
}

void ActionAppMenu::ClearItemsBelowSearchBarForTesting() {
  ClearItemsBelowSearchBar();
}

actions::BaseAction* ActionAppMenu::GetActionForMenuItem(
    views::MenuItemView* menu) const {
  CHECK(menu);
  auto it = command_to_action_map_.find(menu->GetCommand());
  return it != command_to_action_map_.end() ? it->second : nullptr;
}

views::MenuItemView* ActionAppMenu::GetMenuItemForAction(
    actions::BaseAction* action) const {
  if (!root_ || !action) {
    return nullptr;
  }
  for (const auto& [command_id, mapped_action] : command_to_action_map_) {
    if (mapped_action == action) {
      return root_->GetMenuItemByID(command_id);
    }
  }
  return nullptr;
}

AppMenuDragAndDropDelegate* ActionAppMenu::GetDragAndDropDelegate(
    actions::BaseAction* action) const {
  for (actions::BaseAction* curr = action; curr; curr = curr->GetParent()) {
    if (curr->HasPopulateChildActionsCallback()) {
      return curr->GetProperty(AppMenuActionItem::kDragAndDropDelegateKey);
    }
  }
  return nullptr;
}

void ActionAppMenu::RemoveSubmenuActionsFromMap(
    views::MenuItemView* parent_menu_item) {
  if (!parent_menu_item || !parent_menu_item->HasSubmenu()) {
    return;
  }
  for (views::MenuItemView* child :
       parent_menu_item->GetSubmenu()->GetMenuItems()) {
    RemoveSubmenuActionsFromMap(child);
    command_to_action_map_.erase(child->GetCommand());
  }
}

void ActionAppMenu::CancelAndEvaluate(actions::ActionId action_id,
                                      int mouse_event_flags) {
  if (!action_to_execute_on_close_.has_value()) {
    auto action_iterator = command_to_action_map_.find(action_id);
    CHECK(action_iterator != command_to_action_map_.end());
    actions::BaseAction* base_action = action_iterator->second;
    metrics_.LogMenuAction(base_action);
    action_to_execute_on_close_ = {.action_id = action_id,
                                   .context = BuildActionInvocationContext(
                                       base_action, mouse_event_flags)};
    CloseMenu();
  }
}

void ActionAppMenu::ClearItemsBelowSearchBar() {
  CHECK(search_bar_);
  views::SubmenuView* const submenu = root_->GetSubmenu();
  CHECK(submenu);

  // Clear everything except the header (the search row and the rows above it).
  // Once the menu is shown the header lives in the pinned header container, so
  // the whole SubmenuView and the pinned footer are cleared. Before that, the
  // header rows are still at the top of the SubmenuView, so skip past them.
  const auto& children = submenu->children();
  auto first_to_remove = children.begin();
  if (const std::optional<size_t> search_row_index =
          submenu->GetIndexOf(search_bar_->parent())) {
    first_to_remove += *search_row_index + 1;
  }
  std::vector<views::View*> to_remove(first_to_remove, children.end());
  if (const views::MenuScrollViewContainer* const container =
          submenu->scroll_view_container();
      container && container->footer_container()) {
    const auto& footer_children = container->footer_container()->children();
    to_remove.insert(to_remove.end(), footer_children.begin(),
                     footer_children.end());
  }

  for (views::View* child : to_remove) {
    root_->RemoveMenuItem(child);
  }

  // Drop the command mappings of the removed rows. The rows above the search
  // bar, such as notifications, keep theirs.
  base::EraseIf(command_to_action_map_, [this](const auto& entry) {
    return !root_->GetMenuItemByID(entry.first);
  });
  section_header_count_ = 0;
}

void ActionAppMenu::PopulateMenu(views::MenuItemView* view_parent,
                                 actions::BaseAction* base_action_item,
                                 size_t starting_index) {
  const auto& children_action_items =
      base_action_item->GetChildren().children();
  const size_t child_count = children_action_items.size();

  for (size_t i = starting_index; i < child_count; ++i) {
    actions::BaseAction* const child_base = children_action_items[i].get();
    actions::ActionItem* const child_ptr = child_base->GetActionItem();

    if (!child_ptr->GetVisible()) {
      continue;
    }

    const auto display_type =
        child_base->GetProperty(AppMenuActionItem::kDisplayTypeKey);

    if (display_type == AppMenuActionItem::DisplayType::kSearch) {
      PopulateSearchBar(view_parent, child_ptr);
    } else if (display_type == AppMenuActionItem::DisplayType::kFooter) {
      PopulateFooter(view_parent, child_ptr);
    } else if (display_type == AppMenuActionItem::DisplayType::kBlock) {
      PopulateBlockSection(view_parent, child_ptr);
    } else if (display_type == AppMenuActionItem::DisplayType::kDivider) {
      PopulateDivider(view_parent, child_ptr);
    } else if (display_type == AppMenuActionItem::DisplayType::kHeader) {
      PopulateHeader(view_parent, child_base);
    } else if (display_type == AppMenuActionItem::DisplayType::kSection) {
      // Recursively call using the same parent to keep the children in
      // the same menu section.
      PopulateMenu(view_parent, child_base);
    } else {
      auto* const menu_item = AppendMenuItem(child_base, view_parent);
      ConfigureMenuItem(menu_item, child_base,
                        GetCornerRadius(i, children_action_items,
                                        /*is_top=*/true),
                        GetCornerRadius(i, children_action_items,
                                        /*is_top=*/false),
                        ShouldAddTopPadding(i, children_action_items),
                        ShouldAddBottomPadding(i, children_action_items));
      if (display_type == AppMenuActionItem::DisplayType::kCustom) {
        PopulateCustomRow(menu_item, child_base);
      } else if (!child_base->HasPopulateChildActionsCallback()) {
        // Recursively populate static items and static submenus immediately.
        // Dynamic submenus are deferred and populated on demand in
        // WillShowMenu().
        PopulateMenu(menu_item, child_base);
      }
    }
  }
}

void ActionAppMenu::PopulateItemsBelowSearchBar() {
  actions::ActionItem* const app_menu_root = menu_manager_->GetAppMenuRoot();
  const auto& children = app_menu_root->GetChildren().children();
  for (size_t i = 0; i < children.size(); ++i) {
    const auto display_type = children[i]->GetProperty(
        AppMenuActionItem::kDisplayTypeKey);
    if (display_type == AppMenuActionItem::DisplayType::kSearch) {
      PopulateMenu(root_, app_menu_root, i + 1);
      return;
    }
  }
  NOTREACHED();
}

views::MenuItemView* ActionAppMenu::AppendMenuItem(
    actions::BaseAction* base_action_item,
    views::MenuItemView* parent_menu_item,
    std::optional<size_t> index) {
  actions::ActionItem* action_item = base_action_item->GetActionItem();
  CHECK(action_item);
  std::optional<actions::ActionId> action_id = action_item->GetActionId();
  int command_id = action_id.value_or(next_id_++);
  if (command_to_action_map_.contains(command_id)) {
    command_id = next_id_++;
  }

  // Items marked as kCustom lay out their child actions inline in the same row
  // rather than spawning a popup submenu.
  const AppMenuActionItem::DisplayType display_type =
      base_action_item->GetProperty(AppMenuActionItem::kDisplayTypeKey);
  const bool has_submenu =
      display_type != AppMenuActionItem::DisplayType::kCustom &&
      (base_action_item->GetProperty(AppMenuActionItem::kIsSubmenuKey) ||
       !base_action_item->GetChildren().children().empty());

  command_to_action_map_[command_id] = base_action_item;

  const bool is_checkable =
      base_action_item->GetProperty(AppMenuActionItem::kIsCheckableKey);

  views::MenuItemView::Type menu_item_type =
      has_submenu ? views::MenuItemView::Type::kSubMenu
                  : (is_checkable ? views::MenuItemView::Type::kCheckbox
                                  : views::MenuItemView::Type::kNormal);


  views::MenuItemView* menu_item = parent_menu_item->AddMenuItemView(
      std::make_unique<AppMenuItemView>(parent_menu_item, command_id,
                                        menu_item_type, base_action_item,
                                        browser_window_interface_),
      index, /*submenu_background_color=*/std::nullopt,
      /*foreground_color=*/std::nullopt, /*selected_color_id=*/std::nullopt,
      ui::kColorMenuIcon);

  if (display_type == AppMenuActionItem::DisplayType::kNotification) {
    has_notification_header_ = true;
    menu_item->SetIsPinned(true);
  }

  action_view_controller_.CreateActionViewRelationship(
      menu_item, action_item->GetAsWeakPtr());

  command_to_action_map_[command_id] = base_action_item;
  return menu_item;
}

void ActionAppMenu::ConfigureMenuItem(views::MenuItemView* menu_item,
                                      actions::BaseAction* child_base,
                                      int top_radius,
                                      int bottom_radius,
                                      bool add_top_padding,
                                      bool add_bottom_padding) {
  const auto* provider = ChromeLayoutProvider::Get();
  const bool is_notification =
      child_base->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
      AppMenuActionItem::DisplayType::kNotification;
  const int default_icon_size = provider->GetDistanceMetric(
      is_notification ? DISTANCE_ACTION_APP_MENU_NOTIFICATION_ICON_SIZE
                      : DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);

  if (const std::u16string* minor_text =
          child_base->GetProperty(AppMenuActionItem::kMinorTextKey);
      minor_text && !minor_text->empty()) {
    AppMenuMinorTextView::AttachTo(menu_item, *minor_text);
  }

  if (std::u16string* chip_text =
          child_base->GetProperty(AppMenuActionItem::kChipTextKey)) {
    AppMenuChipView::AttachTo(menu_item, *chip_text);
  }

  int target_item_height = 0;
  switch (child_base->GetProperty(AppMenuActionItem::kItemHeightKey)) {
    case AppMenuActionItem::ItemHeight::kCompact:
      target_item_height = provider->GetDistanceMetric(
          DISTANCE_ACTION_APP_MENU_FULL_ITEM_HEIGHT);
      break;
    case AppMenuActionItem::ItemHeight::kMedium:
      target_item_height = provider->GetDistanceMetric(
          DISTANCE_ACTION_APP_MENU_MEDIUM_ITEM_HEIGHT);
      break;
    case AppMenuActionItem::ItemHeight::kLarge:
      target_item_height = provider->GetDistanceMetric(
          DISTANCE_ACTION_APP_MENU_LARGE_ITEM_HEIGHT);
      break;
    case AppMenuActionItem::ItemHeight::kExpanded:
      target_item_height = provider->GetDistanceMetric(
          DISTANCE_ACTION_APP_MENU_EXPANDED_ITEM_HEIGHT);
      break;
  }

  const int content_height =
      std::max(default_icon_size, menu_item->GetIconPreferredSize().height());

  const int vertical_padding = (target_item_height - content_height) / 2;

  menu_item->set_vertical_margin(vertical_padding);

  // Get the styling from the ActionItem and apply it to its menu item.
  if (menu_item->GetParentMenuItem() == root_) {
    const ui::ColorId container_color =
        child_base->GetProperty(AppMenuActionItem::kContainerColorKey);

    views::MenuItemView::MenuItemBackground background(
        container_color, top_radius, bottom_radius,
        /*horizontal_margin=*/
        provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_CONTAINER_MARGIN));
    const int padding = provider->GetDistanceMetric(
        DISTANCE_ACTION_APP_MENU_CONTAINER_VERTICAL_PADDING);
    background.top_padding = add_top_padding ? padding : 0;
    background.bottom_padding = add_bottom_padding ? padding : 0;
    menu_item->SetMenuItemBackground(background);

    // Apply darker hover selection states matching section theme.
    menu_item->SetSelectedColorId(ui::kColorAppMenuRowBackgroundHovered);
  }
}

void ActionAppMenu::PopulateSearchBar(views::MenuItemView* view_parent,
                                      actions::ActionItem* search_action_item) {
  auto* search_item = view_parent->AppendMenuItem(0);
  search_item->SetTriggerActionWithNonIconChildViews(false);
  search_item->set_children_use_full_width(true);
  search_item->set_vertical_margin(0);
  search_item->SetIsPinned(true);

  const auto* provider = ChromeLayoutProvider::Get();
  // The search bar applies its own insets, so only the gap below a
  // notification is set here.
  gfx::Insets margins;
  if (has_notification_header_) {
    margins.set_top(margins.top() +
                    provider->GetDistanceMetric(
                        DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN));
  }

  auto search_bar = std::make_unique<AppMenuSearchBarView>();
  search_bar->SetProperty(views::kMarginsKey, margins);
  search_bar_ = search_item->AddChildView(std::move(search_bar));

  search_controller_ = std::make_unique<AppMenuSearchController>(
      menu_manager_->GetAppMenuRoot(), search_bar_, /*delegate=*/this);
  search_controller_->InitializeSearchIndex();
}

void ActionAppMenu::PopulateFooter(views::MenuItemView* view_parent,
                                   actions::ActionItem* footer_action_item) {
  auto footer_view = std::make_unique<AppMenuFooterView>(
      view_parent, footer_action_item, &action_view_controller_,
      &command_to_action_map_,
      base::BindRepeating(&ActionAppMenu::CancelAndEvaluate,
                          base::Unretained(this)),
      // The footer's callback doesn't take `starting_index`, so wrap
      // PopulateMenu() to use its default.
      base::BindRepeating(
          [](ActionAppMenu* menu, views::MenuItemView* view_parent,
             actions::BaseAction* base_action_item) {
            menu->PopulateMenu(view_parent, base_action_item);
          },
          base::Unretained(this)));

  auto* footer_item = view_parent->AppendMenuItemImpl(
      0, /*label=*/std::u16string(), /*icon=*/ui::ImageModel(),
      views::MenuItemView::Type::kHighlighted);
  footer_item->SetSelectedColorId(ui::kColorMenuBackground);
  footer_item->SetTriggerActionWithNonIconChildViews(false);
  footer_item->set_children_use_full_width(true);
  footer_item->set_vertical_margin(0);
  footer_item->AddChildView(std::move(footer_view));
  footer_item->SetIsPinned(true);
}

void ActionAppMenu::PopulateHeader(views::MenuItemView* view_parent,
                                   actions::BaseAction* header_base_action) {
  actions::ActionItem* const header_action_item =
      header_base_action->GetActionItem();
  auto* const header_menu_item =
      view_parent->AppendTitle(std::u16string(header_action_item->GetText()));
  if (const ui::ElementIdentifier element_id =
          header_base_action->GetProperty(views::kElementIdentifierKey)) {
    header_menu_item->SetProperty(views::kElementIdentifierKey, element_id);
  }
  const int command_id = next_id_++;
  header_menu_item->SetCommand(command_id);
  command_to_action_map_[command_id] = header_base_action;
  const int default_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_HEADER_VERTICAL_MARGIN);
  header_menu_item->set_vertical_margin(default_margin);
  header_menu_item->SetEnabled(false);
  header_menu_item->GetViewAccessibility().SetRoleDescription(
      l10n_util::GetStringUTF16(IDS_AX_ROLE_HEADING));
  header_menu_item->GetViewAccessibility().SetIsEnabled(true);
  if (header_menu_item->GetParentMenuItem() == root_) {
    header_menu_item->SetBorder(
        views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
            INSETS_ACTION_APP_MENU_HEADER)));
    if (section_header_count_++ > 0) {
      header_menu_item->set_top_margin(default_margin * 2);
    }
  }
}

void ActionAppMenu::PopulateBlockSection(
    views::MenuItemView* view_parent,
    actions::ActionItem* block_action_item) {
  auto* block_item = view_parent->AppendMenuItem(0);
  block_item->SetTriggerActionWithNonIconChildViews(false);
  block_item->set_children_use_full_width(true);
  block_item->set_vertical_margin(0);

  const auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets margins =
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_MARGIN);
  if (search_bar_) {
    // The separator's bottom margin is also the gap above search results,
    // which the spec puts closer to the separator than the block buttons.
    margins.set_top(margins.top() +
                    provider->GetDistanceMetric(
                        DISTANCE_ACTION_APP_MENU_SEARCH_BLOCK_MARGIN));
  } else if (has_notification_header_) {
    margins.set_top(margins.top() +
                    provider->GetDistanceMetric(
                        DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN));
  }

  auto block_view = std::make_unique<AppMenuBlockView>(
      block_item, block_action_item, &action_view_controller_,
      &command_to_action_map_,
      base::BindRepeating(&ActionAppMenu::CancelAndEvaluate,
                          base::Unretained(this)));
  block_view->SetProperty(views::kMarginsKey, margins);
  block_item->AddChildView(std::move(block_view));
}

void ActionAppMenu::PopulateCustomRow(views::MenuItemView* view_parent,
                                      actions::BaseAction* custom_action_item) {
  switch (custom_action_item->GetActionItem()->GetActionId().value()) {
    case kActionZoomSubmenu:
      view_parent->AddChildView(std::make_unique<AppMenuZoomView>(
          browser_window_interface_, &action_view_controller_,
          command_to_action_map_, custom_action_item,
          base::BindRepeating(&ActionAppMenu::ExecuteCommand,
                              weak_ptr_factory_.GetWeakPtr())));
      break;

    default:
      NOTREACHED() << "Unsupported custom action id";
  }
}

void ActionAppMenu::PopulateDivider(views::MenuItemView* view_parent,
                                    actions::ActionItem* divider_action_item) {
  const ui::MenuSeparatorType separator_type =
      divider_action_item->GetProperty(AppMenuActionItem::kSeparatorKey);
  views::MenuSeparator* separator =
      view_parent->AppendSeparator(separator_type);
  if (separator_type == ui::MENU_ITEM_SEPARATOR) {
    separator->SetColorId(ui::kColorMenuBackground);
  }
}
