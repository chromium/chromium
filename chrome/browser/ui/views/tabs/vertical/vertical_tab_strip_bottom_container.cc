// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/shared/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

VerticalTabStripBottomContainer::VerticalTabStripBottomContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserWindowInterface* browser,
    base::RepeatingClosure record_new_tab_button_pressed)
    : browser_(browser), root_action_item_(root_action_item) {
  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripBottomContainerElementId);

  auto new_tab_button = std::make_unique<shared::NewTabButton>(
      browser_,
      GetLayoutConstant(LayoutConstant::kVerticalTabStripNewTabButtonSize),
      GetLayoutConstant(LayoutConstant::kVerticalTabStripButtonIconSize));
  new_tab_button->SetOnContextMenuWillShowCallback(base::BindRepeating(
      &VerticalTabStripBottomContainer::OnNewTabButtonContextMenuWillShow,
      base::Unretained(this)));
  new_tab_button->SetOnContextMenuClosedCallback(base::BindRepeating(
      &VerticalTabStripBottomContainer::OnNewTabButtonContextMenuClosed,
      base::Unretained(this)));

  new_tab_button_pressed_subscription_ =
      new_tab_button->RegisterWillInvokeActionCallback(
          record_new_tab_button_pressed);

  new_tab_button_ = AddChildView(std::move(new_tab_button));

  OnCollapseStateChanged(state_controller->GetCollapseState());
  collapsed_state_change_subscription_ =
      state_controller->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripBottomContainer::OnCollapseStateChanged,
          base::Unretained(this)));
}

VerticalTabStripBottomContainer::~VerticalTabStripBottomContainer() = default;


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

void VerticalTabStripBottomContainer::OnCollapseStateChanged(
    tabs::VerticalTabStripCollapseState state) {
  // Updating the styles immediately at start of the animation by including
  // collapsing state.
  UpdateButtonStyles(state != tabs::VerticalTabStripCollapseState::kExpanded);
}

void VerticalTabStripBottomContainer::OnNewTabButtonContextMenuWillShow() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  CHECK(browser_view);
  CHECK(browser_view->tab_strip_view());
  expand_on_hover_lock_ = browser_view->tab_strip_view()->GetExpandOnHoverLock(
      ExpandOnHoverLockType::kKeepCurrentState);
}

void VerticalTabStripBottomContainer::OnNewTabButtonContextMenuClosed() {
  expand_on_hover_lock_.reset();
}

void VerticalTabStripBottomContainer::UpdateButtonStyles(bool collapsed) {
  auto orientation = collapsed ? views::LayoutOrientation::kVertical
                               : views::LayoutOrientation::kHorizontal;

  // Setting button's layout based on collapsed state
  SetOrientation(orientation);
  SetCrossAxisAlignment(collapsed ? views::LayoutAlignment::kStretch
                                  : views::LayoutAlignment::kStart);

  new_tab_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          orientation, views::MinimumFlexSizeRule::kScaleToMinimum,
          collapsed ? views::MaximumFlexSizeRule::kPreferred
                    : views::MaximumFlexSizeRule::kUnbounded,
          false, views::MinimumFlexSizeRule::kPreferred));

  new_tab_button_->SetInsets(GetLayoutInsets(
      collapsed ? LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_COLLAPSED
                : LayoutInset::VERTICAL_TAB_STRIP_BOTTOM_BUTTON_UNCOLLAPSED));
}

BEGIN_METADATA(VerticalTabStripBottomContainer)
END_METADATA
