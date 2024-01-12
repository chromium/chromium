// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"

#include "base/time/time.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_layout.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/style/platform_style.h"

PermissionDashboardView::PermissionDashboardView() {
  SetVisible(false);

  SetLayoutManager(std::make_unique<PermissionDashboardLayout>());

  // Permission request chip should be created the first because it is displayed
  // under all other views.
  request_chip_ = AddChildView(std::make_unique<OmniboxChipButton>(
      OmniboxChipButton::PressedCallback()));

  // Activity indicators chip should be created the last because it is displayed
  // above all other views.
  indicator_chip_ = AddChildView(std::make_unique<OmniboxChipButton>(
      OmniboxChipButton::PressedCallback()));

  // It is unclear which chip will be shown first, hence hide both of them.
  request_chip_->SetVisible(false);
  indicator_chip_->SetVisible(false);
}

PermissionDashboardView::~PermissionDashboardView() = default;

gfx::Size PermissionDashboardView::CalculatePreferredSize() const {
  if (!request_chip_->GetVisible() && !indicator_chip_->GetVisible()) {
    return gfx::Size(0, 0);
  }

  gfx::Size first_chip_size = indicator_chip_->GetPreferredSize();
  if (!request_chip_->GetVisible()) {
    return first_chip_size;
  }

  if (!indicator_chip_->GetVisible()) {
    return request_chip_->GetPreferredSize();
  }

  gfx::Size second_chip_size = request_chip_->GetPreferredSize();
  int width = first_chip_size.width() + second_chip_size.width() -
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_OMNIBOX_CHIPS_OVERLAP);
  int height = first_chip_size.height();

  return gfx::Size(width, height);
}

gfx::Size PermissionDashboardView::GetMinimumSize() const {
  if (!request_chip_->GetVisible() && !indicator_chip_->GetVisible()) {
    return gfx::Size(0, 0);
  }

  if (!request_chip_->GetVisible()) {
    return indicator_chip_->GetMinimumSize();
  }

  if (!indicator_chip_->GetVisible()) {
    return request_chip_->GetMinimumSize();
  }

  gfx::Size first_chip_size = indicator_chip_->GetMinimumSize();
  gfx::Size second_chip_size = request_chip_->GetMinimumSize();
  int width = first_chip_size.width() + second_chip_size.width() -
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_OMNIBOX_CHIPS_OVERLAP);
  int height = first_chip_size.height();

  return gfx::Size(width, height);
}

BEGIN_METADATA(PermissionDashboardView)
END_METADATA
