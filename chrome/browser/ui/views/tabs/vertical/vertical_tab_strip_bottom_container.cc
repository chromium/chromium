// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/bottom_container_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/layout/flex_layout_view.h"

VerticalTabStripBottomContainer::VerticalTabStripBottomContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  // TODO (crbug.com/439961053): Set up a callback subscription for the state
  // controller
  SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // Setting Button's layout based on collapsed state
  SetOrientation(state_controller->IsCollapsed()
                     ? views::LayoutOrientation::kVertical
                     : views::LayoutOrientation::kHorizontal);

  // Flex Specification for uncollapsed state
  views::FlexSpecification uncollapsed_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded, false,
                               views::MinimumFlexSizeRule::kPreferred);

  // TODO(crbug.com/439961435): Add Tab Group Button

  new_tab_button_ = AddChildButtonFor(kActionNewTab);
  new_tab_button_->SetProperty(views::kFlexBehaviorKey,
                               uncollapsed_flex_specification.WithWeight(2));

  new_tab_button_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, GetLayoutConstant(VERTICAL_TAB_STRIP_BOTTOM_BUTTON_PADDING), 0,
          0));
  new_tab_button_->SetProperty(views::kElementIdentifierKey,
                               kNewTabButtonElementId);

  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripBottomContainerElementId);
}

VerticalTabStripBottomContainer::~VerticalTabStripBottomContainer() = default;

views::LabelButton* VerticalTabStripBottomContainer::AddChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<BottomContainerButton> label_button =
      std::make_unique<BottomContainerButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      label_button.get(), action_item->GetAsWeakPtr());

  raw_ptr<BottomContainerButton> raw_label_button =
      AddChildView(std::move(label_button));

  raw_label_button->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);

  return raw_label_button;
}

BEGIN_METADATA(VerticalTabStripBottomContainer)
END_METADATA
