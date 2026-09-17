// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_footer_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_footer_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view_class_properties.h"

AppMenuFooterView::AppMenuFooterView(
    actions::ActionItem* footer_action_item,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::BaseAction>>* command_to_action_map,
    base::RepeatingCallback<void(actions::ActionId)> execute_command_callback) {
  CHECK(footer_action_item);
  CHECK(action_view_controller);
  CHECK(command_to_action_map);
  CHECK(execute_command_callback);

  const auto* provider = ChromeLayoutProvider::Get();

  // The outer footer view arranges the top row, optional separator, and
  // optional bottom row vertically.
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  // Top sub-container: holds the left container, expanding spacer, and right
  // container.
  top_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  top_container_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  top_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  top_container_->SetInsideBorderInsets(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_MARGIN));

  // Left sub-container: holds the Settings and Help action items.
  left_container_ =
      top_container_->AddChildView(std::make_unique<views::BoxLayoutView>());
  left_container_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  left_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  left_container_->SetBetweenChildSpacing(provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_FOOTER_BUTTON_SPACING));

  // Spacer: expands to push the right container to the right edge and
  // absorbs any extra width during menu expansion/localization.
  auto* spacer = top_container_->AddChildView(std::make_unique<views::View>());
  top_container_->SetFlexForView(spacer, 1);

  // Right sub-container: holds the Exit action item.
  right_container_ =
      top_container_->AddChildView(std::make_unique<views::BoxLayoutView>());
  right_container_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  right_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto bottom_container = std::make_unique<views::BoxLayoutView>();
  bottom_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  bottom_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  bottom_container->SetInsideBorderInsets(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_MARGIN));

  // Populate footer buttons from child action items.
  for (const auto& footer_child :
       footer_action_item->GetChildren().children()) {
    actions::ActionItem* footer_child_ptr = footer_child->GetActionItem();
    std::optional<actions::ActionId> action_id =
        footer_child_ptr->GetActionId();
    CHECK(action_id.has_value());

    auto button = std::make_unique<AppMenuFooterButton>();

    action_view_controller->CreateActionViewRelationship(
        button.get(), footer_child_ptr->GetAsWeakPtr());

    (*command_to_action_map)[action_id.value()] = footer_child.get();

    button->SetCallback(
        base::BindRepeating(execute_command_callback, action_id.value()));

    if (std::u16string* text_override =
            footer_child->GetProperty(AppMenuActionItem::kTextOverrideKey)) {
      button->SetText(*text_override);
    }
    if (ui::ImageModel* icon_override =
            footer_child->GetProperty(AppMenuActionItem::kIconOverrideKey)) {
      button->SetImageModel(*icon_override);
    }

    if (action_id.value() == kActionShowManagementPage) {
      auto* button_ptr = bottom_container->AddChildView(std::move(button));
      button_ptr->SetUseRowStyle(true);
      bottom_container->SetFlexForView(button_ptr, 1);
    } else if (action_id.value() == kActionExit) {
      right_container_->AddChildView(std::move(button));
    } else {
      left_container_->AddChildView(std::move(button));
    }
  }

  if (!bottom_container->children().empty()) {
    separator_ = AddChildView(std::make_unique<views::Separator>());
    separator_->SetOrientation(views::Separator::Orientation::kHorizontal);
    separator_->SetColorId(ui::kColorMenuSeparator);
    separator_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            provider->GetDistanceMetric(
                DISTANCE_ACTION_APP_MENU_FOOTER_BOTTOM_CONTAINER_SPACING),
            0, 0, 0));

    bottom_container_ = AddChildView(std::move(bottom_container));
    bottom_container_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            0, 0,
            -provider->GetDistanceMetric(
                DISTANCE_ACTION_APP_MENU_FOOTER_BOTTOM_CONTAINER_SPACING),
            0));
  }
}

AppMenuFooterView::~AppMenuFooterView() = default;

BEGIN_METADATA(AppMenuFooterView)
END_METADATA
