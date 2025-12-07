// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/tabs/vertical/bottom_container_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/flex_layout_view.h"

VerticalTabStripBottomContainer::VerticalTabStripBottomContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserWindowInterface* browser)
    : browser_(browser),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
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

  tab_group_button_ = AddChildButtonFor(kActionTabGroupsMenu);
  tab_group_button_->SetProperty(views::kFlexBehaviorKey,
                                 uncollapsed_flex_specification.WithWeight(1));

  // Creating MenuButtonController because tab_group_button is a LabelButton.
  auto controller = std::make_unique<views::MenuButtonController>(
      tab_group_button_,
      base::BindRepeating(&VerticalTabStripBottomContainer::ShowEverythingMenu,
                          base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          tab_group_button_));

  everything_menu_controller_ = controller.get();

  tab_group_button_->SetButtonController(std::move(controller));

  new_tab_button_ = AddChildButtonFor(kActionNewTab);
  new_tab_button_->SetProperty(views::kFlexBehaviorKey,
                               uncollapsed_flex_specification.WithWeight(2));

  new_tab_button_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, GetLayoutConstant(VERTICAL_TAB_STRIP_BOTTOM_BUTTON_PADDING), 0,
          0));

  tab_group_button_->SetProperty(views::kElementIdentifierKey,
                                 kSavedTabGroupButtonElementId);
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
  // TODO(crbug.com/439961053): Add styling for bottom container buttons in
  // collapsed state.
}

BEGIN_METADATA(VerticalTabStripBottomContainer)
END_METADATA
