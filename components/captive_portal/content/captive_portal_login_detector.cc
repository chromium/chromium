// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/content/captive_portal_login_detector.h"

#include "components/captive_portal/core/captive_portal_types.h"

namespace captive_portal {

CaptivePortalLoginDetector::CaptivePortalLoginDetector(
    CaptivePortalService* captive_portal_service)
    : captive_portal_service_(captive_portal_service),
      is_login_tab_(false),
      first_login_tab_load_(false) {}

CaptivePortalLoginDetector::~CaptivePortalLoginDetector() = default;

void CaptivePortalLoginDetector::OnStoppedLoading() {
  // Do nothing if this is not a login tab, or if this is a login tab's first
  // load.
  if (!is_login_tab_ || first_login_tab_load_) {
    first_login_tab_load_ = false;
    return;
  }

  captive_portal_service_->DetectCaptivePortal();
}

void CaptivePortalLoginDetector::OnCaptivePortalResults(
    CaptivePortalResult previous_result,
    CaptivePortalResult result) {
  if (result != RESULT_BEHIND_CAPTIVE_PORTAL)
    is_login_tab_ = false;
}

void CaptivePortalLoginDetector::SetIsLoginTab() {
  is_login_tab_ = true;
  first_login_tab_load_ = true;
}

}  // namespace captive_portal
