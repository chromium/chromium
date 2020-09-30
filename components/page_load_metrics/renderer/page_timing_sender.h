// Copyright 2017 The Chromium Authors. All rights reserved.
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
      mojom::PageLoadFeaturesPtr new_features,
      std::vector<mojom::ResourceDataUpdatePtr> resources,
      const mojom::FrameRenderDataUpdate& render_data,
      const mojom::CpuTimingPtr& cpu_timing,
      mojom::DeferredResourceCountsPtr new_deferred_resource_data,
      mojom::InputTimingPtr input_timing_delta) = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_SENDER_H_
