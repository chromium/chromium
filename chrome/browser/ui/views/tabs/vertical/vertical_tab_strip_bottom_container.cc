// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/tabs/vertical/bottom_container_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/flex_layout_view.h"

VerticalTabStripBottomContainer::VerticalTabStripBottomContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserWindowInterface* browser)
    : root_action_item_(root_action_item),
      browser_(browser),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripBottomContainerElementId);

  // Flex Specification for uncollapsed state
  uncollapsed_flex_specification_ =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded, false,
                               views::MinimumFlexSizeRule::kPreferred);

  // Flex Specification for collapsed state
  collapsed_flex_specification_ =
      views::FlexSpecification(views::LayoutOrientation::kVertical,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred, false,
                               views::MinimumFlexSizeRule::kPreferred);

  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripBottomContainer::OnCollapsedStateChanged,
          base::Unretained(this)));

  if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
          browser_->GetProfile())) {
    tab_group_button_ = AddChildButtonFor(kActionTabGroupsMenu);

    // Creating MenuButtonController because tab_group_button is a LabelButton.
    auto controller = std::make_unique<views::MenuButtonController>(
        tab_group_button_,
        base::BindRepeating(
            &VerticalTabStripBottomContainer::ShowEverythingMenu,
            base::Unretained(this)),
        std::make_unique<views::Button::DefaultButtonControllerDelegate>(
            tab_group_button_));
    everything_menu_controller_ = controller.get();

    tab_group_button_->SetButtonController(std::move(controller));
    tab_group_button_->SetProperty(views::kElementIdentifierKey,
                                   kSavedTabGroupButtonElementId);
  }

  new_tab_button_ = AddChildButtonFor(kActionNewTab);
  new_tab_button_->SetProperty(views::kElementIdentifierKey,
                               kNewTabButtonElementId);

  UpdateButtonStyles(state_controller);
}

VerticalTabStripBottomContainer::~VerticalTabStripBottomContainer() = default;

BottomContainerButton* VerticalTabStripBottomContainer::AddChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<BottomContainerButton> container_button =
      std::make_unique<BottomContainerButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  raw_ptr<BottomContainerButton> raw_container_button =
      AddChildView(std::move(container_button));

  raw_container_button->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);

  return raw_container_button;
}

void VerticalTabStripBottomContainer::ShowEverythingMenu() {
  if (everything_menu_ && everything_menu_->IsShowing()) {
    return;
  }

  // Creating everything menu.
  everything_menu_ = std::make_unique<tab_groups::STGEverythingMenu>(
      everything_menu_controller_, browser_->GetBrowserForMigrationOnly(),
      tab_groups::STGEverythingMenu::MenuContext::kVerticalTabStrip);

  everything_menu_->RunMenu();
}

void VerticalTabStripBottomContainer::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* controller) {
  UpdateButtonStyles(controller);
}

void VerticalTabStripBottomContainer::UpdateButtonStyles(
    tabs::VerticalTabStripStateController* controller) {
  bool is_collapsed = controller->IsCollapsed();
  auto flex_specification = is_collapsed ? collapsed_flex_specification_
                                         : uncollapsed_flex_specification_;

  // Setting Button's layout based on collapsed state
  SetOrientation(is_collapsed ? views::LayoutOrientation::kVertical
                              : views::LayoutOrientation::kHorizontal);

  if (!tab_group_button_) {
    // If in incognito mode, the tab groups button will not be visible. In that
    // case, we don't want to assign any flat edges or different flex behaviors.
    new_tab_button_->SetProperty(views::kFlexBehaviorKey,
                                 flex_specification.WithWeight(1));
    return;
  }

  // If collapsed, the tab group button and the new tab button share the same
  // weights. The flat edge is inverse to the position: tab group button is
  // placed on top so the flat edge is on the bottom.
  // If uncollapsed, the tab group button and the new tab button are set with
  // weights 1 and 2, respectively. Flat edges should be reset and padding
  // is moved from top to left.

  tab_group_button_->SetProperty(views::kFlexBehaviorKey,
                                 flex_specification.WithWeight(1));
  tab_group_button_->SetFlatEdge(is_collapsed
                                     ? BottomContainerButton::FlatEdge::kBottom
                                     : BottomContainerButton::FlatEdge::kNone);

  new_tab_button_->SetProperty(
      views::kFlexBehaviorKey,
      flex_specification.WithWeight(is_collapsed ? 1 : 2));
  new_tab_button_->SetFlatEdge(is_collapsed
                                   ? BottomContainerButton::FlatEdge::kTop
                                   : BottomContainerButton::FlatEdge::kNone);
  int padding = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripCollapsedBottomButtonPadding);
  new_tab_button_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(is_collapsed ? padding : 0,
                                            is_collapsed ? 0 : padding, 0, 0));
}

BEGIN_METADATA(VerticalTabStripBottomContainer)
END_METADATA
