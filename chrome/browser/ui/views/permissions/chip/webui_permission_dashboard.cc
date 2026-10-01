// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"

#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"

WebUIPermissionDashboard::WebUIPermissionDashboard(LocationBar* location_bar)
    : location_bar_(location_bar),
      request_chip_(location_bar,
                    PermissionChipView::kPermissionRequestChipElementId),
      indicator_chip_(location_bar,
                      PermissionChipView::kIndicatorChipElementId) {}

WebUIPermissionDashboard::~WebUIPermissionDashboard() = default;

void WebUIPermissionDashboard::SetVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }
  is_visible_ = visible;
  UpdateState();
}

bool WebUIPermissionDashboard::GetVisible() const {
  return is_visible_;
}

PermissionChipInterface* WebUIPermissionDashboard::GetRequestChip() {
  return &request_chip_;
}

PermissionChipInterface* WebUIPermissionDashboard::GetIndicatorChip() {
  return &indicator_chip_;
}

views::BubbleAnchor WebUIPermissionDashboard::GetAnchor() {
  // Views anchors to the whole dashboard, whose leading chip is the indicator
  // chip, so the page info bubble ends up in the same place.
  return indicator_chip_.GetAnchor();
}

toolbar_ui_api::mojom::PermissionDashboardStatePtr
WebUIPermissionDashboard::GetState() const {
  auto state = toolbar_ui_api::mojom::PermissionDashboardState::New();
  state->request_chip = request_chip_.GetState();
  state->indicator_chip = indicator_chip_.GetState();
  if (!is_visible_) {
    state->request_chip->is_visible = false;
    state->indicator_chip->is_visible = false;
  }
  state->is_divider_visible =
      state->request_chip->is_visible && state->indicator_chip->is_visible;
  return state;
}

void WebUIPermissionDashboard::UpdateState() {
  location_bar_->OnChanged();
}

void WebUIPermissionDashboard::ResetTabState() {
  request_chip_.InvalidateStateToken();
  indicator_chip_.InvalidateStateToken();
}
