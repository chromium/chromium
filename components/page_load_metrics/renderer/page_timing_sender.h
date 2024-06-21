// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_SENDER_H_

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace page_load_metrics {

// PageTimingSender is an interface that is responsible for sending page load
// timing through IPC.
class PageTimingSender {
 public:
  virtual ~PageTimingSender() = default;
  virtual void SendTiming(
      const mojom::PageLoadTimingPtr& timing,
      const mojom::FrameMetadataPtr& metadata,
      const std::vector<blink::UseCounterFeature>& new_features,
      std::vector<mojom::ResourceDataUpdatePtr> resources,
      const mojom::FrameRenderDataUpdate& render_data,
      const mojom::CpuTimingPtr& cpu_timing,
      mojom::InputTimingPtr input_timing_delta,
      const std::optional<blink::SubresourceLoadMetrics>&
          subresource_load_metrics,
      const mojom::SoftNavigationMetricsPtr& soft_navigation_metrics) = 0;
  virtual void SetUpSmoothnessReporting(
      base::ReadOnlySharedMemoryRegion shared_memory) = 0;
  virtual void SendCustomUserTiming(mojom::CustomUserTimingMarkPtr timing) = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_SENDER_H_
