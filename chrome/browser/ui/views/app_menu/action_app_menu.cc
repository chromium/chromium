// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_block_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_chip_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_minor_text_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_controller.h"
#include "chrome/browser/ui/views/app_menu/app_menu_zoom_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "ui/actions/actions.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/strings/grit/ax_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

// Command-line switch used to inject an initial search query into the app
// menu's search bar when the menu opens. This is used for development and
// testing of the search experience (e.g. testing layout, keyboard navigation,
// and result filtering) before interactive typing or dynamic search is wired
// up.
//
// Usage:
//   --enable-features=AppMenuGlowUp,ChroMenuSearch
//   --app-menu-search-query="<query>"
// Example:
//   --app-menu-search-query="history"
constexpr char kAppMenuSearchQuery[] = "app-menu-search-query";

ui::ImageModel StandardizeMenuIconSize(const ui::ImageModel& icon,
                                       int icon_size) {
  if (icon.IsVectorIcon()) {
    const ui::VectorIconModel& vector_model = icon.GetVectorIcon();
    if (vector_model.icon_size() != icon_size) {
      return ui::ImageModel::FromVectorIcon(*vector_model.vector_icon(),
                                            vector_model.color(), icon_size,
                                            vector_model.badge_icon());
    }
  }
  return icon;
}

bool ShouldShowNewBadge(BrowserWindowInterface* browser_window_interface,
                        const base::Feature& feature) {
  if (auto* const user_education =
          BrowserUserEducationInterface::From(browser_window_interface)) {
    return user_education->MaybeShowNewBadgeFor(feature);
  }
  return UserEducationService::MaybeShowNewBadge(
      browser_window_interface->GetProfile(), feature);
}

bool ShouldRoundBottomCorners(size_t index,
                              const actions::ActionListVector& items) {
  // An item rounds its bottom corners if it is the last non-divider item in
  // its list, if it is a notification item, OR if it is the zoom submenu.
  actions::BaseAction* const base_item = items[index].get();
  if (base_item->GetActionItem()->GetActionId() == kActionZoomSubmenu ||
      base_item->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
          AppMenuActionItem::DisplayType::kNotification) {
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
      return false;
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
    if (SupportsVerticalPadding(prev_base)) {
      return false;
    }
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

void ActionAppMenu::RunMenu(views::MenuButtonController* host) {
  auto root = std::make_unique<views::MenuItemView>(/*delegate=*/this);
  // Stash the raw pointer before transferring the unique_ptr ownership to
  // `menu_runner_`. This allows us to reference the root menu item view later.
  root_ = root.get();

  const auto* provider = ChromeLayoutProvider::Get();
  root_->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM)));
  root_->set_children_use_full_width(true);

  root_->CreateSubmenu();

  const std::string query =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kAppMenuSearchQuery);
  if (query.empty() || !MaybePopulateSearchResults(base::UTF8ToUTF16(query))) {
    PopulateMenu(root_, menu_manager_->GetAppMenuRoot());
    if (search_bar_ && !query.empty()) {
      search_bar_->SetText(base::UTF8ToUTF16(query));
    }
  }

  int32_t types = views::MenuRunner::HAS_MNEMONICS;
  menu_runner_ = std::make_unique<views::MenuRunner>(std::move(root), types);

  metrics_.OnMenuOpened();

  menu_runner_->RunMenuAt(host->button()->GetWidget(), host,
                          host->button()->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kNone,
                          /*native_view_for_gestures=*/gfx::NativeView(),
                          /*corners=*/std::nullopt,
                          "Chrome.AppMenu.MenuHostInitToNextFramePresented");
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

  size_t view_index =
      prev_item
          ? new_parent_menu->GetSubmenu()->GetIndexOf(prev_item).value() + 1
          : 0;

  const auto& target_children = target_parent_action->GetChildren().children();
  views::MenuItemView* new_item =
      AppendMenuItem(action, new_parent_menu, view_index);
  ConfigureMenuItem(new_item, action,
                    ShouldRoundTopCorners(target_index, target_children),
                    ShouldRoundBottomCorners(target_index, target_children),
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
  actions::ActionItem* action_ptr = base_action->GetActionItem();
  CHECK(action_ptr);

  metrics_.LogMenuAction(base_action);

  action_ptr->InvokeAction(
      BuildActionInvocationContext(base_action, mouse_event_flags));
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
  menu_manager_->OnMenuClosed();

  if (action_to_execute) {
    action_to_execute->InvokeAction(std::move(action_context));
  }
  search_controller_.reset();
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
  if (action_iterator == command_to_action_map_.end()) {
    return std::nullopt;
  }

  actions::BaseAction* base_action = action_iterator->second;
  if (base_action->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
          AppMenuActionItem::DisplayType::kHeader &&
      root_ && root_->GetSubmenu()->GetColorProvider()) {
    return root_->GetSubmenu()->GetColorProvider()->GetColor(
        ui::kColorMenuItemForeground);
  }
  return std::nullopt;
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
  views::SubmenuView* submenu = root_->GetSubmenu();
  CHECK(submenu);

  const std::optional<size_t> search_row_index =
      submenu->GetIndexOf(search_bar_->parent());
  CHECK(search_row_index);
  const std::vector<views::View*> to_remove(
      submenu->children().begin() + *search_row_index + 1,
      submenu->children().end());
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
                                 actions::BaseAction* base_action_item) {
  const auto& children_action_items =
      base_action_item->GetChildren().children();
  const size_t child_count = children_action_items.size();

  for (size_t i = 0; i < child_count; ++i) {
    actions::BaseAction* const child_base = children_action_items[i].get();
    actions::ActionItem* const child_ptr = child_base->GetActionItem();

    if (!child_ptr->GetVisible()) {
      continue;
    }

    auto display_type =
        child_base->GetProperty(AppMenuActionItem::kDisplayTypeKey);

    // Individual search result items should always be displayed as normal rows,
    // even if their underlying action was defined with kBlock or kCustom.
    if (search_controller_ &&
        display_type != AppMenuActionItem::DisplayType::kSection &&
        display_type != AppMenuActionItem::DisplayType::kHeader) {
      display_type = AppMenuActionItem::DisplayType::kRow;
    }

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
      ConfigureMenuItem(menu_item,
                        child_base,
                        ShouldRoundTopCorners(i, children_action_items),
                        ShouldRoundBottomCorners(i, children_action_items),
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

  if (display_type == AppMenuActionItem::DisplayType::kNotification) {
    has_notification_header_ = true;
  }

  const std::u16string label =
      has_submenu ? std::u16string(action_item->GetText()) : std::u16string();
  views::MenuItemView* menu_item =
      index.has_value()
          ? parent_menu_item->AddMenuItemAt(
                *index, command_id, label, /*secondary_label=*/std::u16string(),
                /*minor_text=*/std::u16string(),
                /*minor_icon=*/ui::ImageModel(), /*icon=*/ui::ImageModel(),
                menu_item_type, ui::NORMAL_SEPARATOR)
          : parent_menu_item->AppendMenuItemImpl(
                command_id, label, /*icon=*/ui::ImageModel(), menu_item_type);

  action_view_controller_.CreateActionViewRelationship(
      menu_item, action_item->GetAsWeakPtr());

  command_to_action_map_[command_id] = base_action_item;
  return menu_item;
}

void ActionAppMenu::ConfigureMenuItem(views::MenuItemView* menu_item,
                                      actions::BaseAction* child_base,
                                      bool round_top_corners,
                                      bool round_bottom_corners,
                                      bool add_top_padding,
                                      bool add_bottom_padding) {
  actions::ActionItem* const action_item = child_base->GetActionItem();
  CHECK(action_item);

  if (std::u16string* text_override =
          child_base->GetProperty(AppMenuActionItem::kTextOverrideKey)) {
    menu_item->SetTitle(*text_override);
  }

  if (std::u16string* secondary_text =
          child_base->GetProperty(AppMenuActionItem::kSecondaryTextKey)) {
    menu_item->SetSecondaryTitle(*secondary_text);
  }

  const ui::ElementIdentifier element_id =
      child_base->GetProperty(views::kElementIdentifierKey);
  if (element_id) {
    menu_item->SetProperty(views::kElementIdentifierKey, element_id);
  }

  const auto* provider = ChromeLayoutProvider::Get();
  const bool is_notification =
      child_base->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
      AppMenuActionItem::DisplayType::kNotification;
  const int default_icon_size = provider->GetDistanceMetric(
      is_notification ? DISTANCE_ACTION_APP_MENU_NOTIFICATION_ICON_SIZE
                      : DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);

  if (ui::ImageModel* icon_override =
          child_base->GetProperty(AppMenuActionItem::kIconOverrideKey)) {
    menu_item->SetIcon(
        action_item->GetActionId() == kActionProfileSubmenu
            ? *icon_override
            : StandardizeMenuIconSize(*icon_override, default_icon_size));
  } else if (!action_item->GetImage().IsEmpty()) {
    menu_item->SetIcon(
        StandardizeMenuIconSize(action_item->GetImage(), default_icon_size));
  }

  if (ui::ImageModel* minor_icon =
          child_base->GetProperty(AppMenuActionItem::kMinorIconKey)) {
    menu_item->SetMinorIcon(*minor_icon);
  }

  // Display shortcut text if the ActionItem has one.
  const ui::Accelerator& accel = action_item->GetAccelerator();
  if (accel.key_code() != ui::VKEY_UNKNOWN) {
    menu_item->SetMinorText(accel.GetShortcutText());
  }

  if (const std::u16string* minor_text =
          child_base->GetProperty(AppMenuActionItem::kMinorTextKey);
      minor_text && !minor_text->empty()) {
    AppMenuMinorTextView::AttachTo(menu_item, *minor_text);
  }

  if (std::u16string* chip_text =
          child_base->GetProperty(AppMenuActionItem::kChipTextKey)) {
    AppMenuChipView::AttachTo(menu_item, *chip_text);
  }

  if (const base::Feature* new_badge_feature =
          child_base->GetProperty(AppMenuActionItem::kNewBadgeFeatureKey)) {
    const bool show_new_badge =
        ShouldShowNewBadge(browser_window_interface_, *new_badge_feature);
    menu_item->set_new_badge_type(
        show_new_badge ? std::make_optional(ui::NewBadgeType::kNew)
                       : std::nullopt);
  }

  if (child_base->GetProperty(AppMenuActionItem::kIsAlertedKey)) {
    menu_item->SetAlerted();
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
    case AppMenuActionItem::ItemHeight::kExpanded:
      target_item_height = provider->GetDistanceMetric(
          DISTANCE_ACTION_APP_MENU_EXPANDED_ITEM_HEIGHT);
      break;
  }

  const int content_height =
      std::max(default_icon_size, menu_item->GetIconPreferredSize().height());

  const int vertical_padding = (target_item_height - content_height) / 2;

  menu_item->set_vertical_margin(vertical_padding);

  const ui::ColorId container_color =
      child_base->GetProperty(AppMenuActionItem::kContainerColorKey);

  // Get the styling from the ActionItem and apply it to its menu item.
  if (container_color != ui::kColorMenuBackground) {
    const int top_radius = round_top_corners
                               ? provider->GetCornerRadiusMetric(
                                     kActionAppMenuContainerCornerRadius)
                               : 0;
    const int bottom_radius = round_bottom_corners
                                  ? provider->GetCornerRadiusMetric(
                                        kActionAppMenuContainerCornerRadius)
                                  : 0;

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

bool ActionAppMenu::MaybePopulateSearchResults(const std::u16string& query) {
  search_controller_ = std::make_unique<AppMenuSearchController>(
      menu_manager_->GetAppMenuRoot());
  search_controller_->InitializeSearchIndex();
  actions::ActionItem* results = search_controller_->Search(query);
  if (!results) {
    search_controller_.reset();
    return false;
  }

  PopulateSearchBar(root_, nullptr);
  if (search_bar_) {
    search_bar_->SetText(query);
  }
  PopulateMenu(root_, results);
  return true;
}

void ActionAppMenu::PopulateSearchBar(views::MenuItemView* view_parent,
                                      actions::ActionItem* search_action_item) {
  auto* search_item = view_parent->AppendMenuItem(0);
  search_item->SetTriggerActionWithNonIconChildViews(false);
  search_item->set_children_use_full_width(true);
  search_item->set_vertical_margin(0);

  const auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets margins =
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_SEARCH_BAR_MARGIN);
  if (has_notification_header_) {
    margins.set_top(margins.top() +
                    provider->GetDistanceMetric(
                        DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN));
  }

  auto search_bar = std::make_unique<AppMenuSearchBarView>();
  search_bar->SetProperty(views::kMarginsKey, margins);
  search_bar_ = search_item->AddChildView(std::move(search_bar));
}

void ActionAppMenu::PopulateFooter(views::MenuItemView* view_parent,
                                   actions::ActionItem* footer_action_item) {
  auto footer_view = std::make_unique<AppMenuFooterView>(
      view_parent, footer_action_item, &action_view_controller_,
      &command_to_action_map_,
      base::BindRepeating(&ActionAppMenu::CancelAndEvaluate,
                          base::Unretained(this)),
      base::BindRepeating(&ActionAppMenu::PopulateMenu,
                          base::Unretained(this)));

  auto* footer_item = view_parent->AppendMenuItemImpl(
      0, /*label=*/std::u16string(), /*icon=*/ui::ImageModel(),
      views::MenuItemView::Type::kHighlighted);
  footer_item->SetSelectedColorId(ui::kColorMenuBackground);
  footer_item->SetTriggerActionWithNonIconChildViews(false);
  footer_item->set_children_use_full_width(true);
  footer_item->set_vertical_margin(0);
  footer_item->AddChildView(std::move(footer_view));
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
  if (has_notification_header_ && !search_bar_) {
    margins.set_top(margins.top() +
                    provider->GetDistanceMetric(
                        DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN));
  }

  auto block_view = std::make_unique<AppMenuBlockView>(
      block_action_item, &action_view_controller_, &command_to_action_map_,
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
          command_to_action_map_, custom_action_item, metrics_));
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
