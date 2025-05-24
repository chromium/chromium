// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_MEMORY_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_MEMORY_TRACKER_H_

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "content/public/browser/render_frame_host.h"

namespace performance_manager {
class ProcessNode;
}

namespace page_load_metrics {

// PageLoadMetricsMemoryTracker tracks per-frame memory usage by V8 and
// forwards an individual per-frame measurement to the
// MetricsWebContentsObserver associated with the WebContents containing that
// frame.
class PageLoadMetricsMemoryTracker
    : public KeyedService,
      public performance_manager::v8_memory::V8DetailedMemoryObserver {
 public:
  PageLoadMetricsMemoryTracker();
  ~PageLoadMetricsMemoryTracker() override;
  PageLoadMetricsMemoryTracker(const PageLoadMetricsMemoryTracker&) = delete;
  PageLoadMetricsMemoryTracker& operator=(const PageLoadMetricsMemoryTracker&) =
      delete;

  // KeyedService:
  void Shutdown() override;

  // performance_manager::v8_memory::V8DetailedMemoryObserver:
  void OnV8MemoryMeasurementAvailable(
      const performance_manager::ProcessNode* process_node,
      const performance_manager::v8_memory::V8DetailedMemoryProcessData*
          process_data) override;

  // Removes the entry for a deleted frame from `per_frame_memory_usage_map_`.
  void OnRenderFrameDeleted(content::RenderFrameHost* render_frame_host,
                            MetricsWebContentsObserver* observer);

 private:
  int64_t UpdateMemoryUsageAndGetDelta(
      content::RenderFrameHost* render_frame_host,
      uint64_t current_bytes_used);

  // Tracks the most recent per-frame measurements by frame routing id.
  base::flat_map<int, uint64_t> per_frame_memory_usage_map_;

  // Allows receipt of per-frame V8 memory measurements once instantiated
  // and PageLoadMetricsMemoryTracker is added as an observer.
  std::unique_ptr<performance_manager::v8_memory::V8DetailedMemoryRequest>
      memory_request_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_MEMORY_TRACKER_H_
