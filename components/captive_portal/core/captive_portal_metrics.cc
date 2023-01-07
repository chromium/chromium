// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/core/captive_portal_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace captive_portal {

void CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
    CaptivePortalBlockingPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.captive_portal", event,
                            CAPTIVE_PORTAL_BLOCKING_PAGE_EVENT_COUNT);
}

}  // namespace captive_portal
