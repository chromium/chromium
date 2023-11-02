// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_ACCESSIBILITY_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_ACCESSIBILITY_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {

////////////////////////////////////////////////////////////////////////////////
//
// AccessibilityMetricsProvider
//
// A class used to provide frequent signals for AT or accessibility usage
// histograms on Win, Mac and Android, enable accurate counting of unique users.
//
////////////////////////////////////////////////////////////////////////////////
class AccessibilityMetricsProvider : public metrics::MetricsProvider {
 public:
  AccessibilityMetricsProvider();

  AccessibilityMetricsProvider(const AccessibilityMetricsProvider&) = delete;
  AccessibilityMetricsProvider& operator=(const AccessibilityMetricsProvider&) =
      delete;

  ~AccessibilityMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_ACCESSIBILITY_METRICS_PROVIDER_H_
