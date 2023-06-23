// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"

CookieControlsBubbleViewController::CookieControlsBubbleViewController(
    CookieControlsBubbleView* bubble_view,
    content_settings::CookieControlsController* controller)
    : bubble_view_(bubble_view), controller_(controller->AsWeakPtr()) {
  controller_observation_.Observe(controller);
}

CookieControlsBubbleViewController::~CookieControlsBubbleViewController() =
    default;

void CookieControlsBubbleViewController::OnStatusChanged(
    CookieControlsStatus status,
    CookieControlsEnforcement enforcement,
    base::Time expiration) {
  // TODO(1446230): Implement OnStatusChanged.
}

void CookieControlsBubbleViewController::OnSitesCountChanged(
    int allowed_sites,
    int blocked_sites) {
  // TODO(1446230): Implement OnSitesCountChanged
}

void CookieControlsBubbleViewController::OnBreakageConfidenceLevelChanged(
    CookieControlsBreakageConfidenceLevel level) {
  // TODO(1446230): Implement OnBreakageConfidenceLevelChanged.
}
