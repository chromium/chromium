// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
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

namespace {
constexpr int kRegionVeritcalInteriorMargin = 8;
constexpr int kRegionVerticalPadding = 5;

void SetScrollViewProperties(views::ScrollView* scroll_view) {
  scroll_view->SetUseContentsPreferredSize(true);
  scroll_view->SetBackgroundColor(std::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
}
}  // namespace

VerticalTabStripView::VerticalTabStripView() {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  views::ScrollView* pinned_tabs_scroll_view =
      AddChildView(std::make_unique<views::ScrollView>());
  SetScrollViewProperties(pinned_tabs_scroll_view);
  pinned_tabs_container_ = pinned_tabs_scroll_view->SetContents(
      std::make_unique<VerticalPinnedTabContainerView>());

  auto tabs_separator = std::make_unique<views::Separator>();
  tabs_separator->SetColorId(kColorTabDividerFrameActive);
  tabs_separator_ = AddChildView(std::move(tabs_separator));

  views::ScrollView* unpinned_tabs_scroll_view =
      AddChildView(std::make_unique<views::ScrollView>());
  SetScrollViewProperties(unpinned_tabs_scroll_view);
  unpinned_tabs_container_ = unpinned_tabs_scroll_view->SetContents(
      std::make_unique<VerticalUnpinnedTabContainerView>());
}

VerticalTabStripView::~VerticalTabStripView() = default;

views::ProposedLayout VerticalTabStripView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }
  const int region_horizontal_padding =
      GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING);
  views::SizeBounds tab_container_size_bounds =
      size_bounds.Inset(gfx::Insets::VH(kRegionVeritcalInteriorMargin, 0));
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
  views::ScrollView* pinned_tabs_scroll_view =
      views::ScrollView::GetScrollViewForContents(pinned_tabs_container_.get());
  gfx::Rect pinned_container_bounds(
      0, y, tab_container_size_bounds.width().value(),
      pinned_tabs_scroll_view->GetPreferredSize(tab_container_size_bounds)
          .height());
  pinned_container_bounds.set_height(
      std::min(pinned_container_bounds.height(), (remaining_height / 2)));
  layouts.child_layouts.emplace_back(pinned_tabs_scroll_view,
                                     pinned_tabs_scroll_view->GetVisible(),
                                     pinned_container_bounds);

  remaining_height -= pinned_container_bounds.height();
  y += pinned_container_bounds.height() + kRegionVerticalPadding;

  // Place the tabs separator if visible.
  if (tabs_separator_->GetVisible()) {
    int separator_width =
        size_bounds.width().value() - region_horizontal_padding;
    gfx::Rect tabs_separator_bounds(
        0, y, separator_width, tabs_separator_->GetPreferredSize().height());
    layouts.child_layouts.emplace_back(tabs_separator_.get(),
                                       tabs_separator_->GetVisible(),
                                       tabs_separator_bounds);

    y += tabs_separator_bounds.height() + kRegionVerticalPadding;
  }

  // Place the unpinned container.
  gfx::Rect unpinned_container_bounds(
      0, y, tab_container_size_bounds.width().value(), remaining_height);
  views::ScrollView* unpinned_tabs_scroll_view =
      views::ScrollView::GetScrollViewForContents(
          unpinned_tabs_container_.get());
  layouts.child_layouts.emplace_back(unpinned_tabs_scroll_view,
                                     unpinned_tabs_scroll_view->GetVisible(),
                                     unpinned_container_bounds);

  layouts.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  return layouts;
}

void VerticalTabStripView::SetCollapsedState(bool is_collapsed) {
  if (is_collapsed != tabs_separator_->GetVisible()) {
    tabs_separator_->SetVisible(is_collapsed);
  }
}

BEGIN_METADATA(VerticalTabStripView)
END_METADATA
