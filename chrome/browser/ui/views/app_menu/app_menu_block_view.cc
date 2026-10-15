// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_block_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/views/app_menu/app_menu_block_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"

AppMenuBlockView::AppMenuBlockView(
    views::MenuItemView* parent_menu_item,
    actions::ActionItem* block_action_item,
    views::ActionViewController* action_view_controller,
    base::flat_map<int, raw_ptr<actions::BaseAction>>* command_to_action_map,
    ExecuteCommandCallback execute_command_callback) {
  CHECK(parent_menu_item);
  CHECK(block_action_item);
  CHECK(action_view_controller);
  CHECK(command_to_action_map);
  CHECK(execute_command_callback);

  GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);

  const auto* provider = ChromeLayoutProvider::Get();
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  SetBetweenChildSpacing(
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_BLOCK_ROW_SPACING));
  SetDefaultFlex(1);

  for (const auto& block_child : block_action_item->GetChildren().children()) {
    actions::ActionItem* block_child_ptr = block_child->GetActionItem();
    std::optional<actions::ActionId> action_id = block_child_ptr->GetActionId();
    CHECK(action_id.has_value());

    auto button = std::make_unique<AppMenuBlockButton>(parent_menu_item,
                                                       block_child.get());
    action_view_controller->CreateActionViewRelationship(
        button.get(), block_child_ptr->GetAsWeakPtr());
    (*command_to_action_map)[action_id.value()] = block_child.get();

    button->SetCallback(base::BindRepeating(
        [](const ExecuteCommandCallback& callback, actions::ActionId id,
           const ui::Event& event) { callback.Run(id, event.flags()); },
        execute_command_callback, action_id.value()));

    AddChildView(std::move(button));
  }
}

AppMenuBlockView::~AppMenuBlockView() = default;

BEGIN_METADATA(AppMenuBlockView)
END_METADATA
