// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button_border.h"
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

  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripBottomContainer::OnCollapsedStateChanged,
          base::Unretained(this)));

  if (tabs::IsProjectsPanelFeatureEnabled()) {
    tab_group_button_ = AddChildButtonFor(kActionToggleProjectsPanel);
    tab_group_button_->SetProperty(views::kElementIdentifierKey,
                                   kVerticalTabStripProjectsButtonElementId);
  } else if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
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

VerticalTabStripFlatEdgeButton*
VerticalTabStripBottomContainer::AddChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<VerticalTabStripFlatEdgeButton> container_button =
      std::make_unique<VerticalTabStripFlatEdgeButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  VerticalTabStripFlatEdgeButton* raw_container_button =
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

  auto orientation = is_collapsed ? views::LayoutOrientation::kVertical
                                  : views::LayoutOrientation::kHorizontal;

  // Setting button's layout based on collapsed state
  SetOrientation(orientation);

  // If collapsed, the tab group button and the new tab button share the same
  // weights. The flat edge is inverse to the position: tab group button is
  // placed on top so the flat edge is on the bottom.
  // Flat edges should be reset and padding is moved from top to left.

  // If in incognito mode, the tab groups button will not be visible.
  if (tab_group_button_) {
    tab_group_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(orientation,
                                 views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred, false,
                                 views::MinimumFlexSizeRule::kPreferred));
    tab_group_button_->SetFlatEdge(
        is_collapsed ? VerticalTabStripFlatEdgeButton::FlatEdge::kBottom
                     : VerticalTabStripFlatEdgeButton::FlatEdge::kNone);
    tab_group_button_->SetInsets(GetLayoutInsets(
        is_collapsed
            ? LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_COLLAPSED
            : LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_UNCOLLAPSED));
  }

  new_tab_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          orientation, views::MinimumFlexSizeRule::kScaleToZero,
          is_collapsed ? views::MaximumFlexSizeRule::kPreferred
                       : views::MaximumFlexSizeRule::kUnbounded,
          false, views::MinimumFlexSizeRule::kPreferred));
  new_tab_button_->SetFlatEdge(
      is_collapsed ? VerticalTabStripFlatEdgeButton::FlatEdge::kTop
                   : VerticalTabStripFlatEdgeButton::FlatEdge::kNone);
  int padding = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripCollapsedBottomButtonPadding);
  new_tab_button_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(is_collapsed ? padding : 0,
                                            is_collapsed ? 0 : padding, 0, 0));
  new_tab_button_->SetInsets(GetLayoutInsets(
      is_collapsed
          ? LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_COLLAPSED
          : LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_UNCOLLAPSED));
}

BEGIN_METADATA(VerticalTabStripBottomContainer)
END_METADATA
