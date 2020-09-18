// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

TabStripRegionView::TabStripRegionView(std::unique_ptr<TabStrip> tab_strip) {
  views::FlexLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));

  tab_strip_ = tab_strip.get();
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripRegionView::CalculateTabStripAvailableWidth,
                          base::Unretained(this)));
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    views::ScrollView* tab_strip_scroll_container =
        AddChildView(std::make_unique<views::ScrollView>());
    tab_strip_scroll_container->SetBackgroundColor(base::nullopt);
    tab_strip_scroll_container->SetHideHorizontalScrollBar(true);
    tab_strip_container_ = tab_strip_scroll_container;
    tab_strip_scroll_container->SetContents(std::move(tab_strip));
  } else {
    tab_strip_container_ = AddChildView(std::move(tab_strip));
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

  return true;
}

bool TabStripRegionView::IsPositionInWindowCaption(const gfx::Point& point) {
  return IsRectInWindowCaption(gfx::Rect(point, gfx::Size(1, 1)));
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

int TabStripRegionView::CalculateTabStripAvailableWidth() {
  Layout();
  views::SizeBound available_width =
      GetAvailableSize(tab_strip_container_).width();
  // |available_width| might still be unbounded in cases where the tabstrip is
  // hidden (e.g. presentation mode on MacOS). In these cases we don't care
  // about the resulting layout, since the tabstrip is not visible, so we can
  // substitute 0 to ensure that we relayout once the width is defined again.
  return available_width.is_bounded() ? available_width.value() : 0;
}
