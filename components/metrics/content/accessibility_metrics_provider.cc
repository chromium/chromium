// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/accessibility_metrics_provider.h"

#include "content/public/browser/browser_accessibility_state.h"

namespace metrics {

AccessibilityMetricsProvider::AccessibilityMetricsProvider() = default;

AccessibilityMetricsProvider::~AccessibilityMetricsProvider() = default;

void AccessibilityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  content::BrowserAccessibilityState::GetInstance()
      ->UpdateUniqueUserHistograms();
}

}  // namespace metrics
