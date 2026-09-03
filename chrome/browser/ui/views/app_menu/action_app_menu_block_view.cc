// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_block_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_block_button.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/actions/action_view_controller.h"

ActionAppMenuBlockView::ActionAppMenuBlockView(
    actions::ActionItem* block_action_item,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::ActionItem>>* command_to_action_map) {
  CHECK(block_action_item);
  CHECK(action_view_controller);
  CHECK(command_to_action_map);

  const auto* provider = ChromeLayoutProvider::Get();
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  SetInsideBorderInsets(
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_ROW));
  SetBetweenChildSpacing(
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_BLOCK_ROW_SPACING));
  SetDefaultFlex(1);

  for (const auto& block_child : block_action_item->GetChildren().children()) {
    actions::ActionItem* block_child_ptr = block_child->GetActionItem();
    std::optional<actions::ActionId> action_id = block_child_ptr->GetActionId();
    CHECK(action_id.has_value());

    auto button = std::make_unique<ActionAppMenuBlockButton>();
    action_view_controller->CreateActionViewRelationship(
        button.get(), block_child_ptr->GetAsWeakPtr());
    (*command_to_action_map)[action_id.value()] = block_child_ptr;

    if (std::u16string* text_override =
            block_child->GetProperty(ActionAppMenuManager::kTextOverrideKey)) {
      button->SetText(*text_override);
    }

    if (ui::ImageModel* icon_override =
            block_child->GetProperty(ActionAppMenuManager::kIconOverrideKey)) {
      button->SetImageModel(*icon_override);
    }

    AddChildView(std::move(button));
  }
}

ActionAppMenuBlockView::~ActionAppMenuBlockView() = default;

BEGIN_METADATA(ActionAppMenuBlockView)
END_METADATA
