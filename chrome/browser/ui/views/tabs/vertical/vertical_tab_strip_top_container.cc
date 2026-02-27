// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "components/saved_tab_groups/public/features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

VerticalTabStripTopContainer::VerticalTabStripTopContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserWindowInterface* browser)
    : state_controller_(state_controller),
      root_action_item_(root_action_item),
      browser_(browser),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripTopContainerElementId);
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  collapse_button_ = AddChildButtonFor(kActionToggleCollapseVertical);
  collapse_button_->SetProperty(views::kElementIdentifierKey,
                                kVerticalTabStripCollapseButtonElementId);

  if (base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
    unfocus_button_ = AddChildButtonFor(kActionUnfocusTabGroup);
    unfocus_button_->SetProperty(views::kElementIdentifierKey,
                                 kUnfocusTabGroupButtonElementId);
    unfocus_button_->SetVisible(false);
  }

  combo_button_ = AddChildView(std::make_unique<TabStripComboButton>(browser_));
  combo_button_->SetOrientation(
      combo_button_orientation_ = state_controller->IsCollapsed()
                                      ? views::LayoutOrientation::kVertical
                                      : views::LayoutOrientation::kHorizontal);
}

VerticalTabStripTopContainer::~VerticalTabStripTopContainer() = default;

void VerticalTabStripTopContainer::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  combo_button_->SetOrientation(combo_button_orientation_);
}

views::ProposedLayout VerticalTabStripTopContainer::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  gfx::Size host_size =
      gfx::Size(size_bounds.width().is_bounded() ? size_bounds.width().value()
                                                 : parent()->width(),
                toolbar_height_);

  // If we're trying to get the minimum size, it will ask for layout for size
  // bounds {0, 0}, but overflow is based on available size.
  const int available_width =
      host_size.width() > 0
          ? host_size.width()
          : parent()->GetAvailableSize(this).width().value_or(0);

  if (combo_button_
          ->GetPreferredSizeForOrientation(
              views::LayoutOrientation::kHorizontal)
          .width() >= available_width) {
    combo_button_orientation_ = views::LayoutOrientation::kVertical;
    int current_y = 0;

    if (unfocus_button_ && unfocus_button_->GetVisible()) {
      const gfx::Size pref_size = unfocus_button_->GetPreferredSize();
      gfx::Rect bounds(std::max(0, (host_size.width() - pref_size.width()) / 2),
                       current_y, pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(unfocus_button_.get(),
                                        unfocus_button_->GetVisible(), bounds);
      host_size.SetToMax(gfx::Size(bounds.right(), 0));

      current_y +=
          pref_size.height() +
          GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding);
    }

    if (collapse_button_ && collapse_button_->GetVisible()) {
      const gfx::Size pref_size = collapse_button_->GetPreferredSize();
      gfx::Rect bounds(std::max(0, (host_size.width() - pref_size.width()) / 2),
                       current_y, pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(collapse_button_.get(),
                                        collapse_button_->GetVisible(), bounds);
      host_size.SetToMax(gfx::Size(bounds.right(), 0));

      current_y +=
          pref_size.height() +
          GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding);
    }

    if (combo_button_) {
      const gfx::Size pref_size = combo_button_->GetPreferredSizeForOrientation(
          combo_button_orientation_);
      gfx::Rect bounds(std::max(0, (host_size.width() - pref_size.width()) / 2),
                       current_y, pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(combo_button_.get(),
                                        combo_button_->GetVisible(), bounds);
      host_size.SetToMax(gfx::Size(bounds.right(), 0));

      current_y += pref_size.height() +
                   GetLayoutConstant(
                       LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding);
    }

    host_size.SetToMax(gfx::Size(0, current_y));
  } else {
    // If the vertical tab strip is uncollapsed, then lay out the buttons
    // horizontally. The exact y-level of the buttons depends on if they can lay
    // on one line or not.
    combo_button_orientation_ = views::LayoutOrientation::kHorizontal;
    const int padding =
        GetLayoutConstant(LayoutConstant::kVerticalTabStripTopButtonPadding);
    int min_height = 0;
    if (unfocus_button_ && unfocus_button_->GetVisible()) {
      min_height =
          std::max(min_height, unfocus_button_->GetPreferredSize().height());
    }
    if (collapse_button_ && collapse_button_->GetVisible()) {
      min_height =
          std::max(min_height, collapse_button_->GetPreferredSize().height());
    }
    if (combo_button_) {
      min_height = std::max(min_height, combo_button_
                                            ->GetPreferredSizeForOrientation(
                                                combo_button_orientation_)
                                            .height());
    }

    // Guarantee that the height of the container is at least the height of the
    // buttons plus padding. Use the same padding as the toolbar for approximate
    // consistency.
    if (toolbar_height_ == 0) {
      min_height += GetLayoutInsets(TOOLBAR_BUTTON).height();
    }
    host_size.SetToMax(gfx::Size(0, min_height));

    // If there is not enough space for the buttons on a single line with
    // caption buttons, shift them below.
    const bool wrapped_due_to_overflow =
        size_bounds.width().is_bounded() && caption_button_width_ > 0 &&
        GetPreferredWidth() + caption_button_width_ > available_width;

    int y_baseline = host_size.height() / 2;
    // If there is not enough space for all of the buttons to be on the same
    // line as the caption buttons, then we lay them out with collapse_button_
    // anchored to the left. tab_search_ and tab_groups_ are on the right.
    if (wrapped_due_to_overflow) {
      host_size.Enlarge(0,
                        GetLayoutConstant(LayoutConstant::kBookmarkBarHeight));
      y_baseline = toolbar_height_ +
                   (GetLayoutConstant(LayoutConstant::kBookmarkBarHeight) -
                    GetLayoutConstant(
                        LayoutConstant::kBookmarkBarButtonImageLabelPadding)) /
                       2;
    }

    int current_y = 0;
    if (unfocus_button_ && unfocus_button_->GetVisible()) {
      const gfx::Size pref_size = unfocus_button_->GetPreferredSize();
      gfx::Rect bounds(wrapped_due_to_overflow ? 0 : caption_button_width_,
                       std::max(0, y_baseline - pref_size.height() / 2),
                       pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(unfocus_button_.get(),
                                        unfocus_button_->GetVisible(), bounds);
      current_y =
          bounds.bottom() +
          GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding);
    }

    if (collapse_button_ && collapse_button_->GetVisible()) {
      const gfx::Size pref_size = collapse_button_->GetPreferredSize();
      const int x = layout.child_layouts.empty()
                        ? (wrapped_due_to_overflow ? 0 : caption_button_width_)
                        : layout.child_layouts.back().bounds.right() + padding;
      gfx::Rect bounds(x, std::max(0, y_baseline - pref_size.height() / 2),
                       pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(collapse_button_.get(),
                                        collapse_button_->GetVisible(), bounds);
      current_y =
          bounds.bottom() +
          GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding);
    }

    int right_alignment = host_size.width();

    bool wrap_during_animation = available_width < GetPreferredWidth();

    if (combo_button_) {
      const gfx::Size pref_size = combo_button_->GetPreferredSizeForOrientation(
          combo_button_orientation_);
      right_alignment -= pref_size.width();
      gfx::Rect bounds(wrap_during_animation ? 0 : right_alignment,
                       wrap_during_animation
                           ? current_y
                           : std::max(0, y_baseline - pref_size.height() / 2),
                       pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(combo_button_.get(),
                                        combo_button_->GetVisible(), bounds);
    }
  }

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

  return container_button_ptr;
}

TabStripComboButton* VerticalTabStripTopContainer::GetComboButton() {
  return combo_button_.get();
}

TabStripFlatEdgeButton* VerticalTabStripTopContainer::GetTabSearchButton() {
  return combo_button_->end_button();
}

bool VerticalTabStripTopContainer::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (combo_button_ && IsHitInView(combo_button_, point)) {
    return false;
  }

  if (collapse_button_ && IsHitInView(collapse_button_, point)) {
    return false;
  }

  if (unfocus_button_ && IsHitInView(unfocus_button_, point)) {
    return false;
  }

  return true;
}

void VerticalTabStripTopContainer::SetToolbarHeightForLayout(
    int toolbar_height) {
  if (toolbar_height_ == toolbar_height) {
    return;
  }
  toolbar_height_ = toolbar_height;
  InvalidateLayout();
}

void VerticalTabStripTopContainer::SetCaptionButtonWidthForLayout(
    int caption_button_width) {
  if (caption_button_width_ == caption_button_width) {
    return;
  }
  caption_button_width_ = caption_button_width;
  InvalidateLayout();
}

int VerticalTabStripTopContainer::GetPreferredWidth() const {
  int total_width = 0;
  int padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripTopButtonPadding);

  // Combo Button
  total_width += combo_button_
                     ->GetPreferredSizeForOrientation(
                         views::LayoutOrientation::kHorizontal)
                     .width();

  // Collapse Button
  if (collapse_button_ && collapse_button_->GetVisible()) {
    total_width += collapse_button_->GetPreferredSize().width() + padding;
  }

  // Unfocus Button
  if (unfocus_button_ && unfocus_button_->GetVisible()) {
    total_width += unfocus_button_->GetPreferredSize().width() + padding;
  }

  return total_width;
}

BEGIN_METADATA(VerticalTabStripTopContainer)
END_METADATA
