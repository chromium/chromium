// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
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
#include "ui/views/view_utils.h"

namespace {
constexpr int kRegionVeritcalInteriorMargin = 8;
constexpr int kRegionVerticalPadding = 5;

void SetScrollViewProperties(views::ScrollView* scroll_view) {
  scroll_view->SetUseContentsPreferredSize(true);
  scroll_view->SetBackgroundColor(std::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetOverflowGradientMask(
      views::ScrollView::GradientDirection::kVertical);
}
}  // namespace

VerticalTabStripView::VerticalTabStripView(TabCollectionNode* collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetProperty(views::kElementIdentifierKey, kTabStripElementId);

  pinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(pinned_tabs_scroll_view_);

  auto tabs_separator = std::make_unique<views::Separator>();
  tabs_separator->SetColorId(kColorTabDividerFrameActive);
  tabs_separator_ = AddChildView(std::move(tabs_separator));

  unpinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(unpinned_tabs_scroll_view_);

  collection_node->set_add_child_to_node(base::BindRepeating(
      &VerticalTabStripView::AddScrollViewContents, base::Unretained(this)));

  collection_node->set_remove_child_from_node(base::BindRepeating(
      &VerticalTabStripView::RemoveScrollViewContents, base::Unretained(this)));
}

VerticalTabStripView::~VerticalTabStripView() = default;

views::ProposedLayout VerticalTabStripView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }

  const int region_horizontal_padding = GetLayoutConstant(
      is_collapsed_ ? LayoutConstant::kVerticalTabStripCollapsedPadding
                    : LayoutConstant::kVerticalTabStripUncollapsedPadding);

  int y = 0;

  // Allocate the available space between the pinned and unpinned containers so
  // that the pinned container will never take more than half of the available
  // space.
  int remaining_height = size_bounds.height().value() - kRegionVerticalPadding;
  if (tabs_separator_->GetVisible()) {
    remaining_height -=
        tabs_separator_->GetPreferredSize().height() + kRegionVerticalPadding;
  }

  // Place the pinned container.
  views::SizeBounds pinned_tab_container_size_bounds =
      size_bounds.Inset(gfx::Insets::TLBR(kRegionVeritcalInteriorMargin,
                                          region_horizontal_padding,
                                          kRegionVeritcalInteriorMargin, 0));
  gfx::Rect pinned_container_bounds(
      region_horizontal_padding, y,
      pinned_tab_container_size_bounds.width().value(),
      pinned_tabs_scroll_view_
          ->GetPreferredSize(pinned_tab_container_size_bounds)
          .height());
  pinned_container_bounds.set_height(
      std::min(pinned_container_bounds.height(), (remaining_height / 2)));
  layouts.child_layouts.emplace_back(pinned_tabs_scroll_view_.get(),
                                     pinned_tabs_scroll_view_->GetVisible(),
                                     pinned_container_bounds);

  remaining_height -= pinned_container_bounds.height();
  y += pinned_container_bounds.height() + kRegionVerticalPadding;

  // Place the tabs separator if visible.
  const bool has_pinned_tabs = pinned_tabs_container_view_ &&
                               !pinned_tabs_container_view_->children().empty();
  if (is_collapsed_ && has_pinned_tabs) {
    int separator_width =
        size_bounds.width().value() - 2 * region_horizontal_padding;
    gfx::Rect tabs_separator_bounds(
        region_horizontal_padding, y, separator_width,
        tabs_separator_->GetPreferredSize().height());
    layouts.child_layouts.emplace_back(tabs_separator_.get(), true,
                                       tabs_separator_bounds);

    y += tabs_separator_bounds.height() + kRegionVerticalPadding;
  } else {
    layouts.child_layouts.emplace_back(tabs_separator_.get(), false,
                                       gfx::Rect());
  }

  // Place the unpinned container using the entire available width, we do not
  // inset the x value by |region_horizontal_padding| here because, when the tab
  // strip is collapsed, tab groups need to draw the group colored line in this
  // space.
  gfx::Rect unpinned_container_bounds(0, y, size_bounds.width().value(),
                                      remaining_height);
  layouts.child_layouts.emplace_back(unpinned_tabs_scroll_view_.get(),
                                     unpinned_tabs_scroll_view_->GetVisible(),
                                     unpinned_container_bounds);

  layouts.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  layouts.host_size.SetToMax(GetMinimumSize());
  return layouts;
}

gfx::Size VerticalTabStripView::GetMinimumSize() const {
  // The minimum height of the tabstrip should be enough to show a tab and a
  // half, showing a partial overflow so that the user knows the container can
  // be scrolled.
  return gfx::Size(
      GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
      GetLayoutConstant(LayoutConstant::kVerticalTabStripUncollapsedPadding) +
          base::ClampCeil(
              1.5 * GetLayoutConstant(LayoutConstant::kVerticalTabHeight)));
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

BEGIN_METADATA(VerticalTabStripView)
END_METADATA
