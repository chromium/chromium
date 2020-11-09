// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

TabStripRegionView::TabStripRegionView(std::unique_ptr<TabStrip> tab_strip) {
  views::FlexLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal);

  tab_strip_ = tab_strip.get();
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripRegionView::CalculateTabStripAvailableWidth,
                          base::Unretained(this)));
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    views::ScrollView* tab_strip_scroll_container =
        AddChildView(std::make_unique<views::ScrollView>(
            views::ScrollView::ScrollWithLayers::kEnabled));
    tab_strip_scroll_container->SetBackgroundColor(base::nullopt);
    tab_strip_scroll_container->SetHideHorizontalScrollBar(true);
    tab_strip_container_ = tab_strip_scroll_container;
    tab_strip_scroll_container->SetContents(std::move(tab_strip));
  } else {
    tab_strip_container_ = AddChildView(std::move(tab_strip));
  }

  // Allow the |tab_strip_container_| to grow into the free space available in
  // the TabStripRegionView.
  const views::FlexSpecification tab_strip_container_flex_spec =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded);
  tab_strip_container_->SetProperty(views::kFlexBehaviorKey,
                                    tab_strip_container_flex_spec);

  if (base::FeatureList::IsEnabled(features::kTabSearch) &&
      base::FeatureList::IsEnabled(features::kTabSearchFixedEntrypoint) &&
      !tab_strip_->controller()->GetProfile()->IsIncognitoProfile() &&
      tab_strip_->controller()->GetBrowser()->is_type_normal()) {
    // TODO(tluk): |tab_search_container| is only needed here so the tab search
    // button can be vertically centered. This can be removed if FlexLayout is
    // updated to support per-child cross-axis alignment.
    auto* tab_search_container = AddChildView(std::make_unique<views::View>());
    tab_search_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));
    auto* container_layout_manager = tab_search_container->SetLayoutManager(
        std::make_unique<views::FlexLayout>());
    container_layout_manager
        ->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter);

    auto tab_search_button = std::make_unique<TabSearchButton>(tab_strip_);
    tab_search_button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_SEARCH));
    tab_search_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SEARCH));
    tab_search_button_ =
        tab_search_container->AddChildView(std::move(tab_search_button));
  }
}

TabStripRegionView::~TabStripRegionView() = default;

bool TabStripRegionView::IsRectInWindowCaption(const gfx::Rect& rect) {
  const auto get_target_rect = [&](views::View* target) {
    gfx::RectF rect_in_target_coords_f(rect);
    View::ConvertRectToTarget(this, target, &rect_in_target_coords_f);
    return gfx::ToEnclosingRect(rect_in_target_coords_f);
  };

  // Perform a hit test against the |tab_strip_container_| to ensure that the
  // rect is within the visible portion of the |tab_strip_| before calling the
  // tab strip's |IsRectInWindowCaption()|.
  // TODO(tluk): Address edge case where |rect| might partially intersect with
  // the |tab_strip_container_| and the |tab_strip_| but not over the same
  // pixels. This could lead to this returning false when it should be returning
  // true.
  if (tab_strip_container_->HitTestRect(get_target_rect(tab_strip_container_)))
    return tab_strip_->IsRectInWindowCaption(get_target_rect(tab_strip_));

  // The child could have a non-rectangular shape, so if the rect is not in the
  // visual portions of the child view we treat it as a click to the caption.
  for (View* const child : children()) {
    if (child != tab_strip_container_ &&
        child->GetLocalBounds().Intersects(get_target_rect(child))) {
      return !child->HitTestRect(get_target_rect(child));
    }
  }

  return true;
}

bool TabStripRegionView::IsPositionInWindowCaption(const gfx::Point& point) {
  return IsRectInWindowCaption(gfx::Rect(point, gfx::Size(1, 1)));
}

void TabStripRegionView::FrameColorsChanged() {
  if (tab_search_button_)
    tab_search_button_->FrameColorsChanged();
  tab_strip_->FrameColorsChanged();
  SchedulePaint();
}

const char* TabStripRegionView::GetClassName() const {
  return "TabStripRegionView";
}

void TabStripRegionView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

gfx::Size TabStripRegionView::GetMinimumSize() const {
  gfx::Size tab_strip_min_size = tab_strip_->GetMinimumSize();
  // Cap the tabstrip minimum width to a reasonable value so browser windows
  // aren't forced to grow arbitrarily wide.
  const int max_min_width = 520;
  tab_strip_min_size.set_width(
      std::min(max_min_width, tab_strip_min_size.width()));
  return tab_strip_min_size;
}

void TabStripRegionView::OnThemeChanged() {
  View::OnThemeChanged();
  FrameColorsChanged();
}

views::View* TabStripRegionView::GetDefaultFocusableChild() {
  auto* focusable_child = tab_strip_->GetDefaultFocusableChild();
  return focusable_child ? focusable_child
                         : AccessiblePaneView::GetDefaultFocusableChild();
}

void TabStripRegionView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
}

int TabStripRegionView::CalculateTabStripAvailableWidth() {
  // The tab strip can occupy the space not currently taken by its fixed-width
  // sibling views.
  int reserved_width = 0;
  for (View* const child : children()) {
    if (child != tab_strip_container_)
      reserved_width += child->size().width();
  }

  return size().width() - reserved_width;
}
