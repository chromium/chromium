// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_block_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_zoom_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

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

bool ShouldRoundBottomCorners(size_t index,
                              const actions::ActionListVector& items) {
  // An item rounds its bottom corners if it is the last non-divider item in
  // its list OR if its the zoom submenu.
  if (items[index]->GetActionItem()->GetActionId() == kActionZoomSubmenu) {
    return true;
  }
  for (size_t i = index + 1; i < items.size(); ++i) {
    const auto display_type = items[i]->GetActionItem()->GetProperty(
        ActionAppMenuManager::kDisplayTypeKey);
    if (display_type != ActionAppMenuManager::DisplayType::kDivider &&
        display_type != ActionAppMenuManager::DisplayType::kHeader) {
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
    const auto display_type = items[prev_index]->GetActionItem()->GetProperty(
        ActionAppMenuManager::kDisplayTypeKey);
    if (display_type == ActionAppMenuManager::DisplayType::kDivider ||
        display_type == ActionAppMenuManager::DisplayType::kHeader) {
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

  views::SubmenuView* submenu = root_->CreateSubmenu();
  submenu->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_POPUP)));

  PopulateMenu(root_, menu_manager_->GetAppMenuRoot());

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
  actions::ActionItem* action_to_execute = nullptr;
  if (action_to_execute_on_close_) {
    auto action_iterator =
        command_to_action_map_.find(action_to_execute_on_close_.value());
    CHECK(action_iterator != command_to_action_map_.end());
    action_to_execute = action_iterator->second->GetActionItem();
  }

  search_bar_ = nullptr;
  action_to_execute_on_close_.reset();
  command_to_action_map_.clear();
  header_count_ = 0;
  if (on_menu_closed_callback_) {
    on_menu_closed_callback_.Run();
  }
  menu_manager_->GetAppMenuRoot()->ResetActionList();

  if (action_to_execute) {
    action_to_execute->InvokeAction();
  }
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

void ActionAppMenu::CancelAndEvaluate(actions::ActionId action_id) {
  if (!action_to_execute_on_close_.has_value()) {
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

    const ActionAppMenuManager::DisplayType display_type =
        child_ptr->GetProperty(ActionAppMenuManager::kDisplayTypeKey);

    if (display_type == ActionAppMenuManager::DisplayType::kSearch) {
      PopulateSearchBar(view_parent, child_ptr);
    } else if (display_type == ActionAppMenuManager::DisplayType::kFooter) {
      PopulateFooter(view_parent, child_ptr);
    } else if (display_type == ActionAppMenuManager::DisplayType::kBlock) {
      PopulateBlockSection(view_parent, child_ptr);
    } else if (display_type == ActionAppMenuManager::DisplayType::kDivider) {
      PopulateDivider(view_parent, child_ptr);
    } else if (display_type == ActionAppMenuManager::DisplayType::kHeader) {
      PopulateHeader(view_parent, child_ptr);
    } else if (display_type == ActionAppMenuManager::DisplayType::kSection) {
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
      if (display_type == ActionAppMenuManager::DisplayType::kCustom) {
        PopulateCustomRow(menu_item, child_base);
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
  int command_id = action_id.value_or(next_id_++);
  if (command_to_action_map_.contains(command_id)) {
    command_id = next_id_++;
  }

  // Items marked as kCustom lay out their child actions inline in the same row
  // rather than spawning a popup submenu.
  const ActionAppMenuManager::DisplayType display_type =
      action_item->GetProperty(ActionAppMenuManager::kDisplayTypeKey);
  const bool has_submenu =
      display_type != ActionAppMenuManager::DisplayType::kCustom &&
      !base_action_item->GetChildren().children().empty();

  views::MenuItemView* menu_item =
      has_submenu ? parent_menu_item->AppendSubMenu(
                        command_id, std::u16string(action_item->GetText()))
                  : parent_menu_item->AppendMenuItem(command_id);

  action_view_controller_.CreateActionViewRelationship(
      menu_item, action_item->GetAsWeakPtr());
  command_to_action_map_[command_id] = action_item;
  return menu_item;
}

void ActionAppMenu::ConfigureMenuItem(views::MenuItemView* menu_item,
                                      actions::BaseAction* child_base,
                                      bool round_top_corners,
                                      bool round_bottom_corners) {
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

  menu_item->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM)));

  const bool use_expanded_height =
      action_item->GetActionId() == kActionZoomSubmenu;

  const int vertical_padding =
      (provider->GetDistanceMetric(
           use_expanded_height ? DISTANCE_ACTION_APP_MENU_EXPANDED_ITEM_HEIGHT
                               : DISTANCE_ACTION_APP_MENU_FULL_ITEM_HEIGHT) -
       provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE)) /
      2;

  menu_item->set_vertical_margin(vertical_padding);

  const ui::ColorId container_color =
      action_item->GetProperty(ActionAppMenuManager::kContainerColorKey);

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
        container_color, top_radius, bottom_radius, /*horizontal_margin=*/0));

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

  auto search_bar = std::make_unique<ActionAppMenuSearchBarView>();
  search_bar->SetProperty(views::kMarginsKey,
                          ChromeLayoutProvider::Get()->GetInsetsMetric(
                              INSETS_ACTION_APP_MENU_SEARCH_BAR_MARGIN));
  search_bar_ = search_bar.get();
  search_item->AddChildView(std::move(search_bar));
}

void ActionAppMenu::PopulateFooter(views::MenuItemView* view_parent,
                                   actions::ActionItem* footer_action_item) {
  auto* footer_item = view_parent->AppendMenuItem(0);
  footer_item->SetTriggerActionWithNonIconChildViews(false);
  footer_item->set_children_use_full_width(true);
  footer_item->set_vertical_margin(0);

  auto footer_view = std::make_unique<ActionAppMenuFooterView>(
      footer_action_item, &action_view_controller_, &command_to_action_map_,
      base::BindRepeating(&ActionAppMenu::CancelAndEvaluate,
                          base::Unretained(this)));
  footer_view->SetProperty(views::kMarginsKey,
                           ChromeLayoutProvider::Get()->GetInsetsMetric(
                               INSETS_ACTION_APP_MENU_FOOTER_MARGIN));
  footer_item->AddChildView(std::move(footer_view));
}

void ActionAppMenu::PopulateHeader(views::MenuItemView* view_parent,
                                   actions::ActionItem* header_action_item) {
  auto* const header_menu_item =
      view_parent->AppendTitle(std::u16string(header_action_item->GetText()));
  const int default_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_HEADER_VERTICAL_MARGIN);
  header_menu_item->set_vertical_margin(default_margin);
  header_menu_item->SetEnabled(false);
  if (header_menu_item->GetParentMenuItem() == root_) {
    header_menu_item->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    if (header_count_++ > 0) {
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

  block_item->AddChildView(std::make_unique<ActionAppMenuBlockView>(
      block_action_item, &action_view_controller_, &command_to_action_map_,
      base::BindRepeating(&ActionAppMenu::CancelAndEvaluate,
                          base::Unretained(this))));
}

void ActionAppMenu::PopulateCustomRow(views::MenuItemView* view_parent,
                                      actions::BaseAction* custom_action_item) {
  switch (custom_action_item->GetActionItem()->GetActionId().value()) {
    case kActionZoomSubmenu:
      view_parent->AddChildView(std::make_unique<ActionAppMenuZoomView>(
          browser_window_interface_, &action_view_controller_,
          command_to_action_map_, custom_action_item));
      break;

    default:
      NOTREACHED() << "Unsupported custom action id";
  }
}

void ActionAppMenu::PopulateDivider(views::MenuItemView* view_parent,
                                    actions::ActionItem* divider_action_item) {
  view_parent->AppendSeparator(
      divider_action_item->GetProperty(ActionAppMenuManager::kSeparatorKey));
}
