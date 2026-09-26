// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_block_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_chip_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_minor_text_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_zoom_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

ui::ImageModel StandardizeMenuIconSize(const ui::ImageModel& icon) {
  if (icon.IsVectorIcon()) {
    const ui::VectorIconModel& vector_model = icon.GetVectorIcon();
    if (vector_model.icon_size() != ui::SimpleMenuModel::kDefaultIconSize) {
      return ui::ImageModel::FromVectorIcon(
          *vector_model.vector_icon(), vector_model.color(),
          ui::SimpleMenuModel::kDefaultIconSize, vector_model.badge_icon());
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
  actions::ActionItem* const item = items[index]->GetActionItem();
  if (item->GetActionId() == kActionZoomSubmenu ||
      item->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
          AppMenuActionItem::DisplayType::kNotification) {
    return true;
  }
  for (size_t i = index + 1; i < items.size(); ++i) {
    if (!items[i]->GetActionItem()->GetVisible()) {
      continue;
    }
    const auto display_type = items[i]->GetActionItem()->GetProperty(
        AppMenuActionItem::kDisplayTypeKey);
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
    const auto display_type = items[prev_index]->GetActionItem()->GetProperty(
        AppMenuActionItem::kDisplayTypeKey);
    if (display_type == AppMenuActionItem::DisplayType::kDivider ||
        display_type == AppMenuActionItem::DisplayType::kHeader) {
      continue;
    }
    return ShouldRoundBottomCorners(prev_index, items);
  }
  return true;
}

}  // namespace

ActionAppMenu::ActionAppMenu(BrowserWindowInterface* browser_window_interface,
                             base::RepeatingClosure on_menu_closed_callback)
    : browser_window_interface_(browser_window_interface),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)),
      menu_manager_(
          std::make_unique<ActionAppMenuManager>(browser_window_interface)) {
  menu_manager_->CreateMenuHierarchy();
}

ActionAppMenu::~ActionAppMenu() {
  search_bar_ = nullptr;
  command_to_action_map_.clear();
  menu_manager_->GetAppMenuRoot()->ResetActionList();
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

  PopulateMenu(root_, menu_manager_->GetAppMenuRoot());

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
      actions::ActionInvocationContext::Builder()
          .SetProperty(chrome::kDispositionKey,
                       ui::DispositionFromEventFlags(mouse_event_flags))
          .SetProperty(
              AppMenuActionItem::kActionParamKey,
              base_action->GetProperty(AppMenuActionItem::kActionParamKey))
          .Build());
}

void ActionAppMenu::OnMenuClosed(views::MenuItemView* menu) {
  actions::ActionItem* action_to_execute = nullptr;
  int action_param = -1;
  if (action_to_execute_on_close_) {
    auto action_iterator =
        command_to_action_map_.find(action_to_execute_on_close_.value());
    CHECK(action_iterator != command_to_action_map_.end());
    action_to_execute = action_iterator->second->GetActionItem();
    action_param = action_iterator->second->GetProperty(
        AppMenuActionItem::kActionParamKey);
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
    action_to_execute->InvokeAction(
        actions::ActionInvocationContext::Builder()
            .SetProperty(AppMenuActionItem::kActionParamKey, action_param)
            .Build());
  }
}

void ActionAppMenu::WillShowMenu(views::MenuItemView* menu) {
  if (!menu->HasSubmenu() || !menu->GetSubmenu()->GetMenuItems().empty()) {
    return;
  }

  metrics_.OnWillShowSubMenu(menu->GetCommand());

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

  actions::ActionItem* action_ptr = action_iterator->second->GetActionItem();
  CHECK(action_ptr);
  if (action_ptr->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
      AppMenuActionItem::DisplayType::kHeader) {
    return &views::TypographyProvider::Get().GetFont(
        views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5);
  } else if (action_ptr->GetActionId() == kActionProfileSubmenu) {
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

  actions::ActionItem* action_ptr = action_iterator->second->GetActionItem();
  CHECK(action_ptr);
  if (action_ptr->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
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

void ActionAppMenu::CancelAndEvaluate(actions::ActionId action_id) {
  if (!action_to_execute_on_close_.has_value()) {
    auto action_iterator = command_to_action_map_.find(action_id);
    CHECK(action_iterator != command_to_action_map_.end());
    metrics_.LogMenuAction(action_iterator->second);
    action_to_execute_on_close_ = action_id;
    CloseMenu();
  }
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

    const auto display_type =
        child_ptr->GetProperty(AppMenuActionItem::kDisplayTypeKey);

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
      const bool round_top_corners =
          ShouldRoundTopCorners(i, children_action_items);
      const bool round_bottom_corners =
          ShouldRoundBottomCorners(i, children_action_items);
      ConfigureMenuItem(menu_item, child_base, round_top_corners,
                        round_bottom_corners);
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
    views::MenuItemView* parent_menu_item) {
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
      action_item->GetProperty(AppMenuActionItem::kDisplayTypeKey);
  const bool has_submenu =
      display_type != AppMenuActionItem::DisplayType::kCustom &&
      !base_action_item->GetChildren().children().empty();

  command_to_action_map_[command_id] = base_action_item;

  const bool is_checkable =
      action_item->GetProperty(AppMenuActionItem::kIsCheckableKey);

  views::MenuItemView::Type menu_item_type =
      is_checkable ? views::MenuItemView::Type::kCheckbox
                   : views::MenuItemView::Type::kNormal;

  if (display_type == AppMenuActionItem::DisplayType::kNotification) {
    has_notification_header_ = true;
  }

  views::MenuItemView* menu_item =
      has_submenu ? parent_menu_item->AppendSubMenu(
                        command_id, std::u16string(action_item->GetText()))
                  : parent_menu_item->AppendMenuItemImpl(
                        command_id, /*label=*/std::u16string(),
                        /*icon=*/ui::ImageModel(), menu_item_type);

  action_view_controller_.CreateActionViewRelationship(
      menu_item, action_item->GetAsWeakPtr());

  command_to_action_map_[command_id] = base_action_item;
  return menu_item;
}

void ActionAppMenu::ConfigureMenuItem(views::MenuItemView* menu_item,
                                      actions::BaseAction* child_base,
                                      bool round_top_corners,
                                      bool round_bottom_corners) {
  actions::ActionItem* const action_item = child_base->GetActionItem();
  CHECK(action_item);

  if (std::u16string* text_override =
          child_base->GetProperty(AppMenuActionItem::kTextOverrideKey)) {
    menu_item->SetTitle(*text_override);
  }

  const ui::ElementIdentifier element_id =
      child_base->GetProperty(views::kElementIdentifierKey);
  if (element_id) {
    menu_item->SetProperty(views::kElementIdentifierKey, element_id);
  }

  if (ui::ImageModel* icon_override =
          child_base->GetProperty(AppMenuActionItem::kIconOverrideKey)) {
    menu_item->SetIcon(action_item->GetActionId() == kActionProfileSubmenu
                           ? *icon_override
                           : StandardizeMenuIconSize(*icon_override));
  } else if (!action_item->GetImage().IsEmpty()) {
    menu_item->SetIcon(StandardizeMenuIconSize(action_item->GetImage()));
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
          action_item->GetProperty(AppMenuActionItem::kNewBadgeFeatureKey)) {
    const bool show_new_badge =
        ShouldShowNewBadge(browser_window_interface_, *new_badge_feature);
    menu_item->set_new_badge_type(
        show_new_badge ? std::make_optional(ui::NewBadgeType::kNew)
                       : std::nullopt);
  }

  if (child_base->GetProperty(AppMenuActionItem::kIsAlertedKey)) {
    menu_item->SetAlerted();
  }

  const auto* provider = ChromeLayoutProvider::Get();

  const auto item_height =
      action_item->GetProperty(AppMenuActionItem::kItemHeightKey);

  const int target_item_height = provider->GetDistanceMetric(
      item_height == AppMenuActionItem::ItemHeight::kExpanded
          ? DISTANCE_ACTION_APP_MENU_EXPANDED_ITEM_HEIGHT
          : DISTANCE_ACTION_APP_MENU_FULL_ITEM_HEIGHT);

  const int content_height =
      std::max(provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE),
               menu_item->GetIconPreferredSize().height());

  const int vertical_padding = (target_item_height - content_height) / 2;

  menu_item->set_vertical_margin(vertical_padding);

  const ui::ColorId container_color =
      action_item->GetProperty(AppMenuActionItem::kContainerColorKey);

  // Get the styling from the ActionItem and apply it to its menu item.
  if (container_color != ui::kColorMenuBackground) {
    const int top_radius =
        round_top_corners
            ? provider->GetDistanceMetric(
                  DISTANCE_ACTION_APP_MENU_CONTAINER_CORNER_RADIUS)
            : 0;
    const int bottom_radius =
        round_bottom_corners
            ? provider->GetDistanceMetric(
                  DISTANCE_ACTION_APP_MENU_CONTAINER_CORNER_RADIUS)
            : 0;

    menu_item->SetMenuItemBackground(views::MenuItemView::MenuItemBackground(
        container_color, top_radius, bottom_radius,
        /*horizontal_margin=*/
        provider->GetDistanceMetric(
            DISTANCE_ACTION_APP_MENU_CONTAINER_MARGIN)));

    // Apply darker hover selection states matching section theme.
    menu_item->SetSelectedColorId(ui::kColorSysStateHoverOnSubtle);
  }
}

void ActionAppMenu::PopulateSearchBar(views::MenuItemView* view_parent,
                                      actions::ActionItem* search_action_item) {
  auto* search_item = view_parent->AppendMenuItem(0);
  search_item->SetTriggerActionWithNonIconChildViews(false);
  search_item->set_children_use_full_width(true);
  search_item->set_vertical_margin(0);

  auto search_bar = std::make_unique<AppMenuSearchBarView>();
  search_bar->SetProperty(
      views::kMarginsKey,
      ChromeLayoutProvider::Get()->GetInsetsMetric(
          has_notification_header_
              ? INSETS_ACTION_APP_MENU_SEARCH_BAR_WITH_NOTIFICATION_MARGIN
              : INSETS_ACTION_APP_MENU_SEARCH_BAR_MARGIN));
  search_bar_ = search_bar.get();
  search_item->AddChildView(std::move(search_bar));
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
  const int command_id = next_id_++;
  header_menu_item->SetCommand(command_id);
  command_to_action_map_[command_id] = header_base_action;
  const int default_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_HEADER_VERTICAL_MARGIN);
  header_menu_item->set_vertical_margin(default_margin);
  header_menu_item->SetEnabled(false);
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

  auto block_view = std::make_unique<AppMenuBlockView>(
      block_action_item, &action_view_controller_, &command_to_action_map_,
      base::BindRepeating(&ActionAppMenu::CancelAndEvaluate,
                          base::Unretained(this)));
  block_view->SetProperty(
      views::kMarginsKey,
      ChromeLayoutProvider::Get()->GetInsetsMetric(
          has_notification_header_ && !search_bar_
              ? INSETS_ACTION_APP_MENU_BLOCK_WITH_NOTIFICATION_MARGIN
              : INSETS_ACTION_APP_MENU_BLOCK_MARGIN));
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
