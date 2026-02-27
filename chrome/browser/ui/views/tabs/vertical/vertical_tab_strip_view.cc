// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_scroll_bar.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

VerticalTabStripView::VerticalTabStripView(TabCollectionNode* collection_node)
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetProperty(views::kElementIdentifierKey, kTabStripElementId);

  pinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(pinned_tabs_scroll_view_);

  auto tabs_separator = std::make_unique<views::Separator>();
  tabs_separator_ = AddChildView(std::move(tabs_separator));

  unpinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(unpinned_tabs_scroll_view_);

  collection_node->set_add_child_to_node(base::BindRepeating(
      &VerticalTabStripView::AddScrollViewContents, base::Unretained(this)));

  collection_node->set_remove_child_from_node(base::BindRepeating(
      &VerticalTabStripView::RemoveScrollViewContents, base::Unretained(this)));

  callback_subscriptions_.emplace_back(
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabStripView::ResetCollectionNode, base::Unretained(this))));

  SetNotifyEnterExitOnChild(true);
  UpdateColors();
}

VerticalTabStripView::~VerticalTabStripView() = default;

views::ProposedLayout VerticalTabStripView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.width().is_bounded()) {
    return layouts;
  }

  const int region_horizontal_padding = GetLayoutConstant(
      is_collapsed_ ? LayoutConstant::kVerticalTabStripCollapsedPadding
                    : LayoutConstant::kVerticalTabStripUncollapsedPadding);

  const int region_vertical_padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding);

  int y = 0;

  // Determine container preferred heights.
  views::SizeBounds pinned_tab_container_size_bounds =
      size_bounds.Inset(gfx::Insets::TLBR(0, region_horizontal_padding, 0, 0));
  const int pinned_preferred_height =
      pinned_tabs_scroll_view_
          ->GetPreferredSize(pinned_tab_container_size_bounds)
          .height();
  const int unpinned_preferred_height =
      unpinned_tabs_scroll_view_->GetPreferredSize(size_bounds).height();

  const bool should_show_separator = pinned_preferred_height != 0 &&
                                     unpinned_preferred_height != 0 &&
                                     is_collapsed_;

  // If the height is bounded, calculate the available space for laying out the
  // pinned and unpinned containers.
  int remaining_height = 0;
  if (size_bounds.height().is_bounded()) {
    remaining_height = size_bounds.height().value();
    if (pinned_preferred_height != 0 && unpinned_preferred_height != 0) {
      remaining_height -= region_vertical_padding;
    }
    if (should_show_separator) {
      remaining_height -= tabs_separator_->GetPreferredSize().height() +
                          region_vertical_padding;
    }
    // Clamp the remaining height to 0 if we have less space.
    remaining_height = std::max(remaining_height, 0);
  }

  // Place the pinned container.
  int pinned_container_height = pinned_preferred_height;
  if (size_bounds.height().is_bounded()) {
    // The pinned container height should not be larger than half the available
    // space unless the unpinned container will not fill that space. Also make
    // sure the height is at least the minimum.
    pinned_container_height = std::max(
        std::min(pinned_preferred_height,
                 std::max(remaining_height / 2,
                          remaining_height - unpinned_preferred_height)),
        pinned_tabs_container_view_->GetMinimumSize().height());
    remaining_height -= pinned_container_height;
  }
  gfx::Rect pinned_container_bounds(
      region_horizontal_padding, y,
      pinned_tab_container_size_bounds.width().value(),
      pinned_container_height);
  layouts.child_layouts.emplace_back(pinned_tabs_scroll_view_.get(),
                                     pinned_tabs_scroll_view_->GetVisible(),
                                     pinned_container_bounds);

  if (pinned_container_bounds.height()) {
    y += pinned_container_bounds.height();
    // Add padding only if there are pinned and unpinned tabs.
    if (unpinned_preferred_height != 0) {
      y += region_vertical_padding;
    }
  }

  // Place the tabs separator if visible.
  if (should_show_separator) {
    int separator_width =
        size_bounds.width().value() - 2 * region_horizontal_padding;
    int separator_x = region_horizontal_padding;
    if (is_collapsed_) {
      const int collapsed_separator_width = GetLayoutConstant(
          LayoutConstant::kVerticalTabStripCollapsedSeparatorWidth);
      separator_width = collapsed_separator_width;
      separator_x = (size_bounds.width().value() - separator_width) / 2;
    }
    gfx::Rect tabs_separator_bounds(
        separator_x, y, separator_width,
        tabs_separator_->GetPreferredSize().height());
    layouts.child_layouts.emplace_back(tabs_separator_.get(), true,
                                       tabs_separator_bounds);

    y += tabs_separator_bounds.height() + region_vertical_padding;
  } else {
    layouts.child_layouts.emplace_back(tabs_separator_.get(), false,
                                       gfx::Rect());
  }

  // Place the unpinned container using the entire available width, we do not
  // inset the x value by |region_horizontal_padding| here because, when the tab
  // strip is collapsed, tab groups need to draw the group colored line in this
  // space.
  gfx::Rect unpinned_container_bounds(0, y, size_bounds.width().value(),
                                      unpinned_preferred_height);
  if (size_bounds.height().is_bounded()) {
    unpinned_container_bounds.set_height(
        std::max(std::min(unpinned_container_bounds.height(), remaining_height),
                 unpinned_tabs_container_view_->GetMinimumSize().height()));
  }
  layouts.child_layouts.emplace_back(unpinned_tabs_scroll_view_.get(),
                                     unpinned_tabs_scroll_view_->GetVisible(),
                                     unpinned_container_bounds);

  layouts.host_size = gfx::Size(size_bounds.width().value(),
                                unpinned_container_bounds.bottom());
  return layouts;
}

void VerticalTabStripView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalTabStripView::UpdateColors, base::Unretained(this)));
}

void VerticalTabStripView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_entered_tabstrip_time_ = base::TimeTicks::Now();
  has_reported_time_mouse_entered_to_switch_ = false;
}

void VerticalTabStripView::OnMouseExited(const ui::MouseEvent& event) {
  if (!collection_node_) {
    return;
  }

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController();
      hover_card_controller) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kHover);
  }
}

void VerticalTabStripView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (collection_node_ && selection.active_tab_changed() && selection.new_tab) {
    TabCollectionNode* activated_node =
        collection_node_->GetNodeForHandle(selection.new_tab->GetHandle());
    CHECK(activated_node);

    if (pinned_tabs_container_view_->Contains(activated_node->view())) {
      pinned_tabs_scroll_view_->RegisterNextSuccessfulFramePostLayoutCallback(
          base::BindOnce(
              &VerticalTabStripView::DidPresentFramePostActivation,
              base::Unretained(this), pinned_tabs_scroll_view_,
              std::make_unique<views::ViewTracker>(activated_node->view())));
    } else {
      // Views must either be in the pinned or unpinned view trees.
      DCHECK(unpinned_tabs_container_view_->Contains(activated_node->view()));
      unpinned_tabs_scroll_view_->RegisterNextSuccessfulFramePostLayoutCallback(
          base::BindOnce(
              &VerticalTabStripView::DidPresentFramePostActivation,
              base::Unretained(this), unpinned_tabs_scroll_view_,
              std::make_unique<views::ViewTracker>(activated_node->view())));
    }
  }
}

void VerticalTabStripView::RecordMousePressedInTab() {
  if (mouse_entered_tabstrip_time_.has_value() &&
      !has_reported_time_mouse_entered_to_switch_) {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "TabStrip.Vertical.TimeToSwitch",
        base::TimeTicks::Now() - mouse_entered_tabstrip_time_.value());
    has_reported_time_mouse_entered_to_switch_ = true;
  }
}

VerticalPinnedTabContainerView* VerticalTabStripView::GetPinnedTabsContainer() {
  return pinned_tabs_container_view_;
}

VerticalUnpinnedTabContainerView*
VerticalTabStripView::GetUnpinnedTabsContainer() {
  return unpinned_tabs_container_view_;
}

void VerticalTabStripView::SetCollapsedState(bool is_collapsed) {
  if (is_collapsed != is_collapsed_) {
    is_collapsed_ = is_collapsed;
    InvalidateLayout();
  }
}

bool VerticalTabStripView::IsPositionInWindowCaption(const gfx::Point& point) {
  for (views::View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }

    gfx::Point point_in_child = point;
    ConvertPointToTarget(this, child, &point_in_child);
    if (!child->HitTestPoint(point_in_child)) {
      continue;
    }

    auto* scroll_view = views::AsViewClass<views::ScrollView>(child);
    if (!scroll_view) {
      return true;
    }

    if (scroll_view->vertical_scroll_bar()->GetVisible()) {
      gfx::Point point_in_sb = point_in_child;
      ConvertPointToTarget(scroll_view, scroll_view->vertical_scroll_bar(),
                           &point_in_sb);
      if (scroll_view->vertical_scroll_bar()->HitTestPoint(point_in_sb)) {
        return false;
      }
    }

    if (scroll_view->contents()) {
      gfx::Point point_in_content = point_in_child;
      ConvertPointToTarget(scroll_view, scroll_view->contents(),
                           &point_in_content);
      if (scroll_view->contents()->HitTestPoint(point_in_content)) {
        return false;
      }
    }
    return true;
  }

  return true;
}

void VerticalTabStripView::InitializeTabStrip(TabStripModel& tab_strip_model) {
  // TODO(crbug.com/452120900): TabStripModelObserver auto-unregisters in its
  // destructor.
  tab_strip_model.AddObserver(this);
}

views::View* VerticalTabStripView::AddScrollViewContents(
    std::unique_ptr<views::View> view) {
  if (auto* container =
          views::AsViewClass<VerticalUnpinnedTabContainerView>(view.get())) {
    unpinned_tabs_container_view_ = container;
    return unpinned_tabs_scroll_view_->SetContents(std::move(view));
  }
  // |view| should only ever be VerticalUnpinnedTabContainerView or
  // VerticalPinnedTabContainerView.
  auto* container =
      views::AsViewClass<VerticalPinnedTabContainerView>(view.get());
  CHECK(container);
  pinned_tabs_container_view_ = container;
  return pinned_tabs_scroll_view_->SetContents(std::move(view));
}

void VerticalTabStripView::RemoveScrollViewContents(views::View* view) {
  if (views::IsViewClass<VerticalUnpinnedTabContainerView>(view)) {
    unpinned_tabs_container_view_ = nullptr;
    unpinned_tabs_scroll_view_->SetContents(nullptr);
    return;
  }
  if (views::IsViewClass<VerticalPinnedTabContainerView>(view)) {
    pinned_tabs_container_view_ = nullptr;
    pinned_tabs_scroll_view_->SetContents(nullptr);
    return;
  }
  // |view| should only ever be VerticalUnpinnedTabContainerView or
  // VerticalPinnedTabContainerView.
  NOTREACHED();
}

void VerticalTabStripView::SetScrollViewProperties(
    views::ScrollView* scroll_view) {
  scroll_view->SetUseContentsPreferredSize(true);
  scroll_view->SetBackgroundColor(std::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetOverflowGradientMask(
      views::ScrollView::GradientDirection::kVertical);
  CHECK(collection_node_);
  scroll_view->SetVerticalScrollBar(std::make_unique<VerticalTabStripScrollBar>(
      collection_node_->GetController()->GetStateController()));
  callback_subscriptions_.emplace_back(scroll_view->AddContentsScrolledCallback(
      base::BindRepeating(&VerticalTabStripView::HideHoverCardOnScroll,
                          base::Unretained(this))));
}

void VerticalTabStripView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabStripView::DidPresentFramePostActivation(
    views::ScrollView* scroll_view,
    std::unique_ptr<views::ViewTracker> view_tracker) {
  views::View* const activated_view = view_tracker->view();

  // Guard against views being removed from the tree between frames.
  if (!activated_view || !Contains(activated_view)) {
    return;
  }

  // Dragging a view out of the visible bounds will trigger a scroll naturally.
  if (collection_node_ &&
      collection_node_->GetController()->GetDragHandler().IsDragging()) {
    return;
  }

  // Get view bounds in its contents coordinates.
  gfx::Rect activated_view_bounds =
      GetVerticalTabStripViewTargetBounds(activated_view);

  // Proceed up the hierarchy until the content view is reached, iteratively
  // adjusting target view bounds.
  for (views::View* v = activated_view->parent(); v != scroll_view->contents();
       v = v->parent()) {
    activated_view_bounds =
        views::View::ConvertRectToTarget(v, v->parent(), activated_view_bounds);
  }

  // Get the visible bounds of the content view.
  const gfx::Rect visible_contents_rect = scroll_view->GetVisibleRect();

  // Determine the adjustment required to fit the activated view into the
  // visible content view bounds.
  gfx::Rect adjusted_activated_view_bounds = activated_view_bounds;
  adjusted_activated_view_bounds.AdjustToFit(visible_contents_rect);

  // Calculate the required scroll offset for the visible content bounds (the
  // reverse of the activated view adjustment).
  int diff = activated_view_bounds.y() - adjusted_activated_view_bounds.y();

  scroll_view->ScrollByOffset({0, static_cast<float>(diff)});
}

void VerticalTabStripView::UpdateColors() {
  tabs_separator_->SetColorId(IsFrameActive() ? kColorTabDividerFrameActive
                                              : kColorTabDividerFrameInactive);
}

bool VerticalTabStripView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

void VerticalTabStripView::HideHoverCardOnScroll() {
  if (!collection_node_) {
    return;
  }

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController();
      hover_card_controller && hover_card_controller->IsHoverCardVisible()) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kAnimating);
  }
}

BEGIN_METADATA(VerticalTabStripView)
END_METADATA
