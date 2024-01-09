// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_layout.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

PermissionDashboardLayout::PermissionDashboardLayout() = default;

PermissionDashboardLayout::~PermissionDashboardLayout() = default;

views::ProposedLayout PermissionDashboardLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  int x = 0;

  auto views = host_view()->children();
  DCHECK(views.size() == 2);

  views::View* indicator_chip_view = views[1];
  if (indicator_chip_view->GetVisible()) {
    gfx::Size preferred_size = indicator_chip_view->GetPreferredSize();
    gfx::Rect proposed_bounds = gfx::Rect(gfx::Point(0, 0), preferred_size);
    views::SizeBounds available_bounds = views::SizeBounds(preferred_size);

    layout.child_layouts.push_back({indicator_chip_view,
                                    indicator_chip_view->GetVisible(),
                                    proposed_bounds, available_bounds});

    // `indicator_chip_view` and `request_chip_view` overlaps to create an
    // illusion that `request_chip_view` is placed under `indicator_chip_view`.
    // To achieve that move `request_chip_view` x coordinate to the left by
    // subtracting the overlap value from it.
    x = preferred_size.width() - ChromeLayoutProvider::Get()->GetDistanceMetric(
                                     DISTANCE_OMNIBOX_CHIPS_OVERLAP);
  }

  views::View* request_chip_view = views[0];
  if (request_chip_view->GetVisible()) {
    gfx::Size preferred_size = request_chip_view->GetPreferredSize();
    gfx::Rect proposed_bounds = gfx::Rect(gfx::Point(x, 0), preferred_size);
    views::SizeBounds available_bounds = views::SizeBounds(preferred_size);

    layout.child_layouts.push_back({request_chip_view,
                                    request_chip_view->GetVisible(),
                                    proposed_bounds, available_bounds});
  }

  return layout;
}
