// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

PermissionDashboardController::PermissionDashboardController(
    Browser* browser,
    LocationBarView* location_bar_view,
    PermissionDashboardView* permission_dashboard_view)
    : location_bar_view_(location_bar_view),
      permission_dashboard_view_(permission_dashboard_view) {
  request_chip_controller_ = std::make_unique<ChipController>(
      browser, permission_dashboard_view_->GetRequestChip(),
      permission_dashboard_view_);
}

PermissionDashboardController::~PermissionDashboardController() = default;
