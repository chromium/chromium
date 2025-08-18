// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kRegionVeritcalInteriorMargin = 8;
constexpr int kRegionVerticalPadding = 5;
constexpr int kRegionHorizontalPadding = 12;
}  // namespace

VerticalTabStripRegionView::VerticalTabStripRegionView(
    tabs::VerticalTabStripStateController* state_controller) {
  SetBackground(views::CreateSolidBackground(ui::kColorFrameActive));
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  // Create child views.
  top_button_container_ = AddChildView(std::make_unique<views::View>());

  top_button_separator_ = AddChildView(std::make_unique<views::Separator>());
  top_button_separator_->SetColorId(kColorTabDividerFrameActive);

  pinned_tabs_container_ = AddChildView(std::make_unique<views::View>());

  auto tabs_separator = std::make_unique<views::Separator>();
  tabs_separator->SetColorId(kColorTabDividerFrameActive);
  // The tabs separator is only visible if in the collapsed state.
  tabs_separator->SetVisible(state_controller->IsCollapsed());
  tabs_separator_ = AddChildView(std::move(tabs_separator));

  unpinned_tabs_container_ = AddChildView(std::make_unique<views::View>());

  segmented_button_ = AddChildView(std::make_unique<views::View>());

  gemini_button_ = AddChildView(std::make_unique<views::View>());

  resize_area_ = AddChildView(std::make_unique<views::ResizeArea>(this));

  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnStateChanged(base::BindRepeating(
          &VerticalTabStripRegionView::OnCollapsedStateChanged,
          base::Unretained(this)));
}

VerticalTabStripRegionView::~VerticalTabStripRegionView() = default;

views::ProposedLayout VerticalTabStripRegionView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }
  views::SizeBounds default_size_bounds = size_bounds.Inset(
      gfx::Insets::VH(kRegionVeritcalInteriorMargin, kRegionHorizontalPadding));
  views::SizeBounds tab_container_size_bounds = size_bounds.Inset(
      gfx::Insets::TLBR(kRegionVeritcalInteriorMargin, kRegionHorizontalPadding,
                        kRegionVeritcalInteriorMargin, 0));

  int y = kRegionVeritcalInteriorMargin;

  // First place all top views with fixed sizes.
  // Place |top_button_container_|.
  gfx::Rect top_container_bounds(
      kRegionHorizontalPadding, y, default_size_bounds.width().value(),
      top_button_container_->GetPreferredSize(default_size_bounds).height());
  layouts.child_layouts.emplace_back(top_button_container_.get(),
                                     top_button_container_->GetVisible(),
                                     top_container_bounds);

  y += top_container_bounds.height() + kRegionVerticalPadding;

  // Place the |top_button_separator_|.
  gfx::Rect top_button_separator_bounds(
      kRegionHorizontalPadding, y, default_size_bounds.width().value(),
      top_button_separator_->GetPreferredSize(default_size_bounds).height());
  layouts.child_layouts.emplace_back(top_button_separator_.get(),
                                     top_button_separator_->GetVisible(),
                                     top_button_separator_bounds);

  y += top_button_separator_bounds.height() + kRegionVerticalPadding;

  // Next place all the bottom views with fixed sizes.
  int bottom = size_bounds.height().value() - kRegionVeritcalInteriorMargin;

  // // Place the |gemini_button_|.
  int gemini_button_height =
      gemini_button_->GetPreferredSize(default_size_bounds).height();
  gfx::Rect gemini_button_bounds(
      kRegionHorizontalPadding, bottom - gemini_button_height,
      default_size_bounds.width().value(), gemini_button_height);
  layouts.child_layouts.emplace_back(
      gemini_button_.get(), gemini_button_->GetVisible(), gemini_button_bounds);

  bottom -= gemini_button_height + kRegionVerticalPadding;

  // Place the |segmented_button_|.
  int segmented_button_height =
      segmented_button_->GetPreferredSize(default_size_bounds).height();
  gfx::Rect segmented_button_bounds(
      kRegionHorizontalPadding, bottom - segmented_button_height,
      default_size_bounds.width().value(), segmented_button_height);
  layouts.child_layouts.emplace_back(segmented_button_.get(),
                                     segmented_button_->GetVisible(),
                                     segmented_button_bounds);

  bottom -= segmented_button_bounds.height() + kRegionVerticalPadding;

  // Calculate the remaining available space and allocate it between the pinned
  // and unpinned containers so that the pinned container will never take more
  // than half of the remaining space.
  int remaining_tabs_flex_height = bottom - y - kRegionVerticalPadding;
  if (tabs_separator_->GetVisible()) {
    remaining_tabs_flex_height -=
        tabs_separator_->GetPreferredSize(default_size_bounds).height() +
        kRegionVerticalPadding;
  }

  // Place the pinned container.
  gfx::Rect pinned_container_bounds(
      kRegionHorizontalPadding, y, tab_container_size_bounds.width().value(),
      pinned_tabs_container_->GetPreferredSize(tab_container_size_bounds)
          .height());
  pinned_container_bounds.set_height(std::min(
      pinned_container_bounds.height(), (remaining_tabs_flex_height / 2)));
  layouts.child_layouts.emplace_back(pinned_tabs_container_.get(),
                                     pinned_tabs_container_->GetVisible(),
                                     pinned_container_bounds);

  remaining_tabs_flex_height -= pinned_container_bounds.height();
  y += pinned_container_bounds.height() + kRegionVerticalPadding;

  // Place the tabs separator if visible.
  if (tabs_separator_->GetVisible()) {
    gfx::Rect tabs_separator_bounds(
        kRegionHorizontalPadding, y, default_size_bounds.width().value(),
        tabs_separator_->GetPreferredSize(default_size_bounds).height());
    layouts.child_layouts.emplace_back(tabs_separator_.get(),
                                       tabs_separator_->GetVisible(),
                                       tabs_separator_bounds);

    y += tabs_separator_bounds.height() + kRegionVerticalPadding;
  }

  // Place the unpinned container.
  gfx::Rect unpinned_container_bounds(kRegionHorizontalPadding, y,
                                      tab_container_size_bounds.width().value(),
                                      remaining_tabs_flex_height);
  layouts.child_layouts.emplace_back(unpinned_tabs_container_.get(),
                                     unpinned_tabs_container_->GetVisible(),
                                     unpinned_container_bounds);

  // Place the |resize_area_|.
  gfx::Rect resize_area_bounds(size_bounds.width().value() - kResizeAreaWidth,
                               0, kResizeAreaWidth,
                               size_bounds.height().value());
  layouts.child_layouts.emplace_back(
      resize_area_.get(), resize_area_->GetVisible(), resize_area_bounds);

  layouts.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  return layouts;
}

void VerticalTabStripRegionView::OnResize(int resize_amount,
                                          bool done_resizing) {}

void VerticalTabStripRegionView::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* state_controller) {
  if (state_controller->IsCollapsed() != tabs_separator_->GetVisible()) {
    tabs_separator_->SetVisible(state_controller->IsCollapsed());
  }
}

BEGIN_METADATA(VerticalTabStripRegionView)
END_METADATA
