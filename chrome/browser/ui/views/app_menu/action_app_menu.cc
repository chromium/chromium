// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_zoom_view.h"
#include "chrome/browser/ui/views/app_menu/block_menu_entry_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

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
  PopulateMenu(root_, menu_manager_->GetAppMenuRoot());

  root_->set_children_use_full_width(true);
  views::SubmenuView* submenu = root_->CreateSubmenu();

  gfx::Insets insets = provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_POPUP);
  if (base::FeatureList::IsEnabled(features::kChroMenuSearch)) {
    auto search_bar = std::make_unique<ActionAppMenuSearchBarView>();
    search_bar_ = search_bar.get();
    submenu->AddChildViewAt(std::move(search_bar), 0);
    insets.set_top(4);
  }

  submenu->SetBorder(views::CreateEmptyBorder(insets));
  submenu->set_minimum_preferred_width(
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_MINIMUM_WIDTH));

  int32_t types = views::MenuRunner::HAS_MNEMONICS;
  menu_runner_ = std::make_unique<views::MenuRunner>(std::move(root), types);

  // TODO(crbug.com/526712325): Create duplicate app menu histograms specific to
  // the Block Style ChroMenu.
  menu_runner_->RunMenuAt(host->button()->GetWidget(), host,
                          host->button()->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kNone);
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

  actions::ActionItem* action_ptr = action_iterator->second->GetActionItem();
  CHECK(action_ptr);

  action_ptr->InvokeAction();
}

void ActionAppMenu::OnMenuClosed(views::MenuItemView* menu) {
  search_bar_ = nullptr;
  command_to_action_map_.clear();
  if (on_menu_closed_callback_) {
    on_menu_closed_callback_.Run();
  }
  menu_manager_->GetAppMenuRoot()->ResetActionList();
}

const gfx::FontList* ActionAppMenu::GetLabelFontList(int id) const {
  if (id == ui::MenuModel::kTitleId) {
    return &views::TypographyProvider::Get().GetFont(
        views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5);
  }
  return nullptr;
}

std::optional<SkColor> ActionAppMenu::GetLabelColor(int id) const {
  if (id == ui::MenuModel::kTitleId && root_ && root_->HasSubmenu() &&
      root_->GetSubmenu()->GetColorProvider()) {
    return root_->GetSubmenu()->GetColorProvider()->GetColor(
        ui::kColorMenuItemForeground);
  }
  return std::nullopt;
}

void ActionAppMenu::PopulateMenu(views::MenuItemView* view_parent,
                                 actions::BaseAction* base_action_item) {
  const auto& children_action_items =
      base_action_item->GetChildren().children();
  const size_t child_count = children_action_items.size();

  for (size_t i = 0; i < child_count; ++i) {
    actions::BaseAction* const child_base = children_action_items[i].get();
    actions::ActionItem* const child_ptr = child_base->GetActionItem();
    if (!child_ptr) {
      continue;
    }

    const ActionAppMenuManager::DisplayType display_type =
        child_ptr->GetProperty(ActionAppMenuManager::kDisplayTypeKey);

    if (display_type == ActionAppMenuManager::DisplayType::kFooter) {
      PopulateFooter(view_parent, child_ptr);
    } else if (display_type == ActionAppMenuManager::DisplayType::kBlock) {
      PopulateBlockMenuItem(view_parent, child_ptr);
    } else if (display_type == ActionAppMenuManager::DisplayType::kDivider) {
      view_parent->AppendSeparator();
    } else if (display_type == ActionAppMenuManager::DisplayType::kSection) {
      auto* section_header_menu_item =
          view_parent->AppendTitle(std::u16string(child_ptr->GetText()));
      ConfigureSectionHeader(section_header_menu_item);
      // Recursively call using the same parent to keep the children in
      // the same menu section as the header.
      PopulateMenu(view_parent, child_base);
    } else {
      auto* const menu_item = AppendMenuItem(child_base, view_parent);
      ConfigureMenuItem(menu_item, child_base, i == 0, i == child_count - 1);
      if (child_ptr->GetActionId() == kActionZoomSubmenu) {
        menu_item->AddChildView(std::make_unique<ActionAppMenuZoomView>(
            browser_window_interface_, &action_view_controller_,
            command_to_action_map_, child_base));
      } else {
        // Recursively populate the menu with the base action item's children.
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
  const int command_id = action_id.value_or(next_id_++);

  // Even though the zoom menu item has children, it should not be treated
  // as a submenu because its children are laid out within the same top
  // level menu item.
  const bool is_zoom_menu_item =
      action_item->GetActionId() == kActionZoomSubmenu;
  const bool has_children =
      !is_zoom_menu_item && !base_action_item->GetChildren().children().empty();

  views::MenuItemView* menu_item =
      has_children ? parent_menu_item->AppendSubMenu(
                         command_id, std::u16string(action_item->GetText()))
                   : parent_menu_item->AppendMenuItem(command_id);

  action_view_controller_.CreateActionViewRelationship(
      menu_item, action_item->GetAsWeakPtr());
  command_to_action_map_[command_id] = action_item;
  return menu_item;
}

void ActionAppMenu::ConfigureSectionHeader(
    views::MenuItemView* header_menu_item) {
  header_menu_item->SetEnabled(false);
  header_menu_item->set_vertical_margin(
      views::LayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_ACTION_APP_MENU_HEADER_VERTICAL_MARGIN));
}

void ActionAppMenu::ConfigureMenuItem(views::MenuItemView* menu_item,
                                      actions::BaseAction* child_base,
                                      bool is_first_item,
                                      bool is_last_item) {
  if (std::u16string* text_override =
          child_base->GetProperty(ActionAppMenuManager::kTextOverrideKey)) {
    menu_item->SetTitle(*text_override);
  }

  actions::ActionItem* const action_item = child_base->GetActionItem();
  CHECK(action_item);
  if (ui::ImageModel* icon_override =
          child_base->GetProperty(ActionAppMenuManager::kIconOverrideKey)) {
    menu_item->SetIcon(StandardizeMenuIconSize(*icon_override));
  } else if (!action_item->GetImage().IsEmpty()) {
    menu_item->SetIcon(StandardizeMenuIconSize(action_item->GetImage()));
  }

  // Display shortcut text if the ActionItem has one.
  const ui::Accelerator& accel = action_item->GetAccelerator();
  if (accel.key_code() != ui::VKEY_UNKNOWN) {
    menu_item->SetMinorText(accel.GetShortcutText());
  }

  const auto* provider = ChromeLayoutProvider::Get();

  // Set the border radius depending on the position a menu item has in
  // its section.
  const int top_radius =
      is_first_item ? provider->GetDistanceMetric(
                          DISTANCE_ACTION_APP_MENU_CONTAINER_CORNER_RADIUS)
                    : 0;
  const int top_padding =
      is_first_item
          ? provider->GetDistanceMetric(
                DISTANCE_ACTION_APP_MENU_ITEM_FIRST_TOP_PADDING)
          : provider->GetDistanceMetric(
                DISTANCE_ACTION_APP_MENU_ITEM_DEFAULT_VERTICAL_MARGIN);

  const int bottom_radius =
      is_last_item ? provider->GetDistanceMetric(
                         DISTANCE_ACTION_APP_MENU_CONTAINER_CORNER_RADIUS)
                   : 0;

  const int bottom_padding =
      is_last_item ? provider->GetDistanceMetric(
                         DISTANCE_ACTION_APP_MENU_ITEM_LAST_BOTTOM_PADDING)
                   : provider->GetDistanceMetric(
                         DISTANCE_ACTION_APP_MENU_ITEM_DEFAULT_VERTICAL_MARGIN);

  menu_item->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM)));

  const ui::ColorId container_color =
      action_item->GetProperty(ActionAppMenuManager::kContainerColorKey);

  // Get the styling from the ActionItem and apply it to its menu item.
  if (container_color != ui::kColorMenuBackground) {
    menu_item->SetContainerStyle(container_color, top_radius, bottom_radius,
                                 top_padding, bottom_padding);
    // Apply darker hover selection states matching section theme.
    menu_item->SetSelectedColorId(ui::kColorSysStateHoverOnSubtle);
  }
}

void ActionAppMenu::PopulateFooter(views::MenuItemView* view_parent,
                                   actions::ActionItem* footer_action_item) {
  auto* footer_item = view_parent->AppendMenuItem(0);
  footer_item->SetTriggerActionWithNonIconChildViews(false);
  footer_item->set_children_use_full_width(true);

  footer_item->AddChildView(std::make_unique<ActionAppMenuFooterView>(
      footer_action_item, &action_view_controller_, &command_to_action_map_));
}

void ActionAppMenu::PopulateBlockMenuItem(
    views::MenuItemView* view_parent,
    actions::ActionItem* block_action_item) {
  auto* block_item = view_parent->AppendMenuItem(0);
  block_item->SetTriggerActionWithNonIconChildViews(false);
  block_item->set_children_use_full_width(true);

  const auto* provider = ChromeLayoutProvider::Get();
  auto row_view = std::make_unique<views::BoxLayoutView>();
  row_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  row_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  row_view->SetInsideBorderInsets(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_ROW));
  row_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_BLOCK_ROW_SPACING));
  row_view->SetDefaultFlex(1);

  for (const auto& block_child : block_action_item->GetChildren().children()) {
    actions::ActionItem* block_child_ptr = block_child->GetActionItem();
    std::optional<actions::ActionId> action_id = block_child_ptr->GetActionId();
    CHECK(action_id.has_value());

    auto button = std::make_unique<BlockMenuEntryButton>();
    action_view_controller_.CreateActionViewRelationship(
        button.get(), block_child_ptr->GetAsWeakPtr());
    command_to_action_map_[action_id.value()] = block_child_ptr;

    if (std::u16string* text_override =
            block_child->GetProperty(ActionAppMenuManager::kTextOverrideKey)) {
      button->SetText(*text_override);
    }

    if (ui::ImageModel* icon_override =
            block_child->GetProperty(ActionAppMenuManager::kIconOverrideKey)) {
      button->SetImageModel(*icon_override);
    }

    row_view->AddChildView(std::move(button));
  }
  block_item->AddChildView(std::move(row_view));
}
