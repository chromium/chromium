// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_WEBUI_PERMISSION_DASHBOARD_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_WEBUI_PERMISSION_DASHBOARD_H_

#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_interface.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"

class WebUILocationBar;

class WebUIPermissionDashboard : public PermissionDashboardInterface {
 public:
  explicit WebUIPermissionDashboard(WebUILocationBar* location_bar);
  ~WebUIPermissionDashboard() override;

  // PermissionDashboardInterface:
  void SetVisible(bool visible) override;
  bool GetVisible() const override;
  PermissionChipInterface* GetRequestChip() override;
  PermissionChipInterface* GetIndicatorChip() override;
  views::BubbleAnchor GetAnchor() override;

  toolbar_ui_api::mojom::PermissionDashboardStatePtr GetState() const;

  WebUIPermissionChip* request_chip() { return &request_chip_; }
  WebUIPermissionChip* indicator_chip() { return &indicator_chip_; }

 private:
  void UpdateState();

  raw_ptr<WebUILocationBar> location_bar_;
  bool is_visible_ = false;

  WebUIPermissionChip request_chip_;
  WebUIPermissionChip indicator_chip_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_WEBUI_PERMISSION_DASHBOARD_H_
