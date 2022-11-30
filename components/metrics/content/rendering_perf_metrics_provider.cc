// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/rendering_perf_metrics_provider.h"

#include "gpu/config/gpu_util.h"

namespace metrics {

RenderingPerfMetricsProvider::RenderingPerfMetricsProvider() = default;

RenderingPerfMetricsProvider::~RenderingPerfMetricsProvider() = default;

void RenderingPerfMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  gpu::RecordDevicePerfInfoHistograms();
}

}  // namespace metrics
