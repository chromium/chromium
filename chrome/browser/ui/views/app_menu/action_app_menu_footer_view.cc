// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"

ActionAppMenuFooterView::ActionAppMenuFooterView(
    actions::ActionItem* footer_action_item,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::ActionItem>>* command_to_action_map) {
  CHECK(footer_action_item);
  CHECK(action_view_controller);
  CHECK(command_to_action_map);

  const auto* provider = ChromeLayoutProvider::Get();

  // The outer footer view arranges the subcontainers and an expanding spacer.
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetInsideBorderInsets(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER));

  // Left sub-container: holds the Settings and Help action items.
  auto* left_container = AddChildView(std::make_unique<views::BoxLayoutView>());
  left_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  left_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  left_container->SetBetweenChildSpacing(provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_FOOTER_BUTTON_SPACING));

  // Spacer: expands to push the right container to the right edge and
  // absorbs any extra width during menu expansion/localization.
  auto* spacer = AddChildView(std::make_unique<views::View>());
  SetFlexForView(spacer, 1);

  // Right sub-container: holds the Exit action item.
  auto* right_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  right_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  right_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Populate footer buttons from child action items.
  for (const auto& footer_child :
       footer_action_item->GetChildren().children()) {
    actions::ActionItem* footer_child_ptr = footer_child->GetActionItem();
    std::optional<actions::ActionId> action_id =
        footer_child_ptr->GetActionId();
    CHECK(action_id.has_value());

    auto button = std::make_unique<ActionAppMenuFooterButton>();
    action_view_controller->CreateActionViewRelationship(
        button.get(), footer_child_ptr->GetAsWeakPtr());
    (*command_to_action_map)[action_id.value()] = footer_child_ptr;

    if (action_id.value() == kActionExit) {
      right_container->AddChildView(std::move(button));
    } else {
      left_container->AddChildView(std::move(button));
    }
  }
}

ActionAppMenuFooterView::~ActionAppMenuFooterView() = default;

BEGIN_METADATA(ActionAppMenuFooterView)
END_METADATA
