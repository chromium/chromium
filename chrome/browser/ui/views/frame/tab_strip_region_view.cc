// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

TabStripRegionView::TabStripRegionView() {
  views::FlexLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification::ForSizeRule(
                      views::MinimumFlexSizeRule::kScaleToZero,
                      views::MaximumFlexSizeRule::kUnbounded));
}

TabStripRegionView::~TabStripRegionView() {}

const char* TabStripRegionView::GetClassName() const {
  return "TabStripRegionView";
}

void TabStripRegionView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}
