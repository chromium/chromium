// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTopButtonContainerHeight = 28;
constexpr int kTopButtonPadding = 4;
}  // namespace

VerticalTabStripTopContainer::VerticalTabStripTopContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item)
    : state_controller_(state_controller),
      root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  tab_search_button_ = AddChildButtonFor(kActionTabSearch);
  tab_search_button_->SetProperty(views::kElementIdentifierKey,
                                  kTabSearchButtonElementId);

  collapse_button_ = AddChildButtonFor(kActionToggleCollapseVertical);
  collapse_button_->SetProperty(views::kElementIdentifierKey,
                                kVerticalTabStripCollapseButtonElementId);

  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripTopContainerElementId);
}

VerticalTabStripTopContainer::~VerticalTabStripTopContainer() = default;

// TODO(crbug.com/445528000): Update height calculation after child components
// are added
views::ProposedLayout VerticalTabStripTopContainer::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  gfx::Size host_size =
      gfx::Size(size_bounds.width().is_bounded() ? size_bounds.width().value()
                                                 : parent()->width(),
                kTopButtonContainerHeight);

  CHECK(tab_search_button_);
  CHECK(collapse_button_);

  const gfx::Size tab_search_button_pref_size =
      tab_search_button_->GetPreferredSize();
  const gfx::Size collapse_button_pref_size =
      collapse_button_->GetPreferredSize();

  // If there is not enough space for the buttons on a single line with caption
  // buttons, shift them below.
  if (exclusion_width_ > 0 &&
      exclusion_width_ + tab_search_button_pref_size.width() +
              kTopButtonPadding + collapse_button_pref_size.width() >
          host_size.width()) {
    host_size.Enlarge(0, toolbar_height_);
  }

  int current_x = host_size.width();
  int current_y = host_size.height();

  // Calculate bounds to right-align the button horizontally and center it
  // vertically within the available space.
  gfx::Rect tab_search_button_bounds(
      current_x - tab_search_button_pref_size.width(),
      current_y -
          (kTopButtonContainerHeight + tab_search_button_pref_size.height()) /
              2,
      tab_search_button_pref_size.width(),
      tab_search_button_pref_size.height());
  layout.child_layouts.emplace_back(
      tab_search_button_.get(), tab_search_button_->GetVisible(),
      tab_search_button_bounds, views::SizeBounds(tab_search_button_pref_size));

  current_x = tab_search_button_bounds.x() - kTopButtonPadding;

  // Re-calculate bounds based on new x value, offset by the tab search button.
  gfx::Rect collapse_button_bounds(
      current_x - collapse_button_pref_size.width(),
      current_y -
          (kTopButtonContainerHeight + collapse_button_pref_size.height()) / 2,
      collapse_button_pref_size.width(), collapse_button_pref_size.height());
  layout.child_layouts.emplace_back(
      collapse_button_.get(), collapse_button_->GetVisible(),
      collapse_button_bounds, views::SizeBounds(collapse_button_pref_size));

  layout.host_size = host_size;
  return layout;
}

views::LabelButton* VerticalTabStripTopContainer::AddChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<TopContainerButton> container_button =
      std::make_unique<TopContainerButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  TopContainerButton* container_button_ptr =
      AddChildView(std::move(container_button));

  container_button_ptr->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  return container_button_ptr;
}

bool VerticalTabStripTopContainer::IsPositionInWindowCaption(
    const gfx::Point& point) {
  const auto is_hit_in_view = [&](views::View* target) {
    gfx::Point point_in_target = point;
    View::ConvertPointToTarget(this, target, &point_in_target);
    return target->HitTestPoint(point_in_target);
  };

  if (tab_search_button_ && is_hit_in_view(tab_search_button_)) {
    return false;
  }

  if (collapse_button_ && is_hit_in_view(collapse_button_)) {
    return false;
  }

  return true;
}

void VerticalTabStripTopContainer::SetToolbarHeightForLayout(
    const int toolbar_height) {
  toolbar_height_ = toolbar_height;
}

void VerticalTabStripTopContainer::SetExclusionWidthForLayout(
    const int exclusion_width) {
  exclusion_width_ = exclusion_width;
}

BEGIN_METADATA(VerticalTabStripTopContainer)
END_METADATA
