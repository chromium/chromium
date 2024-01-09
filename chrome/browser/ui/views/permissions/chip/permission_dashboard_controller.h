// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"

class Browser;
class LocationBarView;
class ChipController;

class PermissionDashboardController {
 public:
  PermissionDashboardController(
      Browser* browser,
      LocationBarView* location_bar_view,
      PermissionDashboardView* permission_dashboard_view);

  ~PermissionDashboardController();
  PermissionDashboardController(const PermissionDashboardController&) = delete;
  PermissionDashboardController& operator=(
      const PermissionDashboardController&) = delete;

  ChipController* request_chip_controller() {
    return request_chip_controller_.get();
  }

  PermissionDashboardView* permission_dashboard_view() {
    return permission_dashboard_view_;
  }

 private:
  raw_ptr<LocationBarView> location_bar_view_;
  raw_ptr<PermissionDashboardView> permission_dashboard_view_;

  std::unique_ptr<ChipController> request_chip_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_CONTROLLER_H_
