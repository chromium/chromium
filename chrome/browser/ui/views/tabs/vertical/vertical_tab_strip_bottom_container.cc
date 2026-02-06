// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

VerticalTabStripBottomContainer::VerticalTabStripBottomContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    base::RepeatingClosure record_new_tab_button_pressed)
    : root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripBottomContainerElementId);

  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripBottomContainer::OnCollapsedStateChanged,
          base::Unretained(this)));

  new_tab_button_ = AddChildButtonFor(kActionNewTab);
  new_tab_button_->SetProperty(views::kElementIdentifierKey,
                               kNewTabButtonElementId);
  new_tab_button_pressed_subscription_ =
      new_tab_button_->RegisterWillInvokeActionCallback(
          record_new_tab_button_pressed);

  UpdateButtonStyles(state_controller);
}

VerticalTabStripBottomContainer::~VerticalTabStripBottomContainer() = default;

TabStripFlatEdgeButton* VerticalTabStripBottomContainer::AddChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<TabStripFlatEdgeButton> container_button =
      std::make_unique<TabStripFlatEdgeButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  TabStripFlatEdgeButton* raw_container_button =
      AddChildView(std::move(container_button));

  raw_container_button->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);

  const int raw_container_button_size =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripNewTabButtonSize);
  raw_container_button->SetPreferredSize(
      gfx::Size(raw_container_button_size, raw_container_button_size));

  raw_container_button->SetIconSize(
      GetLayoutConstant(LayoutConstant::kVerticalTabStripTopButtonIconSize));

  return raw_container_button;
}

bool VerticalTabStripBottomContainer::IsPositionInWindowCaption(
    const gfx::Point& point) {
  for (views::View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }
    gfx::Point point_in_child = point;
    views::View::ConvertPointToTarget(this, child, &point_in_child);
    if (child->HitTestPoint(point_in_child)) {
      return false;
    }
  }
  return true;
}

void VerticalTabStripBottomContainer::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* controller) {
  UpdateButtonStyles(controller);
}

void VerticalTabStripBottomContainer::UpdateButtonStyles(
    tabs::VerticalTabStripStateController* controller) {
  bool is_collapsed = controller->IsCollapsed();

  auto orientation = is_collapsed ? views::LayoutOrientation::kVertical
                                  : views::LayoutOrientation::kHorizontal;

  // Setting button's layout based on collapsed state
  SetOrientation(orientation);
  SetCrossAxisAlignment(is_collapsed ? views::LayoutAlignment::kStretch
                                     : views::LayoutAlignment::kStart);

  new_tab_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          orientation, views::MinimumFlexSizeRule::kScaleToMinimum,
          is_collapsed ? views::MaximumFlexSizeRule::kPreferred
                       : views::MaximumFlexSizeRule::kUnbounded,
          false, views::MinimumFlexSizeRule::kPreferred));

  new_tab_button_->SetInsets(GetLayoutInsets(
      is_collapsed
          ? LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_COLLAPSED
          : LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_UNCOLLAPSED));
}

BEGIN_METADATA(VerticalTabStripBottomContainer)
END_METADATA
