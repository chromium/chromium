// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

WebUIPermissionDashboard::WebUIPermissionDashboard(
    WebUILocationBar* location_bar)
    : location_bar_(location_bar),
      request_chip_(location_bar),
      indicator_chip_(location_bar) {}

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
  // The WebUI element tracker registration happens asynchronously over Mojo.
  // If a permission is requested during browser startup, we might attempt to
  // anchor the bubble before the WebUI has finished registering the tracked
  // element, causing GetAnchorOrNull() to return nullptr. We fallback to the
  // main window contents view to prevent a crash during this sub-millisecond
  // race condition.
  if (ui::TrackedElement* element = location_bar_->GetAnchorOrNull()) {
    return views::BubbleAnchor(element);
  }
  return views::BubbleAnchor(
      location_bar_->GetLocationBarWidget()->GetContentsView());
}

toolbar_ui_api::mojom::PermissionDashboardStatePtr
WebUIPermissionDashboard::GetState() const {
  if (!is_visible_) {
    return nullptr;
  }

  auto state = toolbar_ui_api::mojom::PermissionDashboardState::New();
  state->request_chip = request_chip_.GetState();
  state->indicator_chip = indicator_chip_.GetState();
  state->is_divider_visible =
      request_chip_.GetVisible() && indicator_chip_.GetVisible();
  return state;
}

void WebUIPermissionDashboard::UpdateState() {
  location_bar_->OnChanged();
}
