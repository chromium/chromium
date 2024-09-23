// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

class PermissionChipView;

// UI component for activity indicators and chip button located in the omnibox.
class PermissionDashboardView : public views::View {
  METADATA_HEADER(PermissionDashboardView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDashboardElementId);
  PermissionDashboardView();
  PermissionDashboardView(const PermissionDashboardView& button) = delete;
  PermissionDashboardView& operator=(const PermissionDashboardView& button) =
      delete;
  ~PermissionDashboardView() override;

  PermissionChipView* GetRequestChip() { return secondary_chip_; }
  PermissionChipView* GetIndicatorChip() { return anchored_chip_; }
  views::View* GetDividerView() { return chip_divider_view_; }

  void SetDividerBackgroundColor(SkColor background_color);
  void UpdateDividerViewVisibility();

  // views::View.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::View::Views GetChildrenInZOrder() override;

 private:
  // This chip is used to display in-use left-hand side activity indicators.
  raw_ptr<PermissionChipView> anchored_chip_ = nullptr;
  // This chip is used to display a permission request and blockade indicator.
  raw_ptr<PermissionChipView> secondary_chip_ = nullptr;
  // TODO(crbug.com/324449830): Remove `chip_divider_view_` and
  // implement a custom background for `secondary_chip_` with a concave oval
  // from the left side.
  raw_ptr<views::View> chip_divider_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_VIEW_H_
