// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_LAYOUT_H_

#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"

// PermissionDashboardLayout is a LayoutManager that is designed to be used only
// for `PermissionDashboardView`. The layout manager positions views in a row
// with a small overlay so that the first view is displayed on top of the second
// view.
class PermissionDashboardLayout : public views::LayoutManagerBase {
 public:
  PermissionDashboardLayout();

  PermissionDashboardLayout(const PermissionDashboardLayout&) = delete;
  PermissionDashboardLayout& operator=(const PermissionDashboardLayout&) =
      delete;

  ~PermissionDashboardLayout() override;

  // LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_LAYOUT_H_
