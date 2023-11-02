// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_RENDERING_PERF_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_RENDERING_PERF_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {

// RenderingPerfMetricsProvider provides metrics related to rendering
// performance.
class RenderingPerfMetricsProvider : public MetricsProvider {
 public:
  RenderingPerfMetricsProvider();

  RenderingPerfMetricsProvider(const RenderingPerfMetricsProvider&) = delete;
  RenderingPerfMetricsProvider& operator=(const RenderingPerfMetricsProvider&) =
      delete;

  ~RenderingPerfMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_RENDERING_PERF_METRICS_PROVIDER_H_
