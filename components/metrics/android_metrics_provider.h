// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_ANDROID_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_ANDROID_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {

class ChromeUserMetricsExtension;

// AndroidMetricsProvider provides Android-specific stability metrics.
class AndroidMetricsProvider : public metrics::MetricsProvider {
 public:
  AndroidMetricsProvider();

  AndroidMetricsProvider(const AndroidMetricsProvider&) = delete;
  AndroidMetricsProvider& operator=(const AndroidMetricsProvider&) = delete;

  ~AndroidMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_ANDROID_METRICS_PROVIDER_H_
