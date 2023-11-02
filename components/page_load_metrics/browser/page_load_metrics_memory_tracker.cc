// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"

#include <map>

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace features {

// Enables or disables per-frame memory monitoring.
BASE_FEATURE(kV8PerFrameMemoryMonitoring,
             "V8PerFrameMemoryMonitoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace page_load_metrics {

namespace {

// WeakPtrs cannot be used as the key to a map without a custom comparator, so
// we use a raw pointer for the map key and bundle the WeakPtr in a struct
// to be used as the value.
struct ObserverWeakPtrAndMemoryUpdates {
  base::WeakPtr<MetricsWebContentsObserver> weak_ptr;
  std::vector<MemoryUpdate> updates;
  ObserverWeakPtrAndMemoryUpdates(
      base::WeakPtr<MetricsWebContentsObserver> wk_ptr,
      MemoryUpdate update)
      : weak_ptr(wk_ptr) {
    updates.emplace_back(update);
  }
};

}  // namespace

// Results of the V8PerAdFrameMemoryPollParamsStudy indicated that at the
// ~99.8th percentile, collecting at 10-second or 60-second intervals
// yields nearly equivalent results, as does using kBounded or kLazy mode.
// As there is about 10% to 20% overhead total GC time, we chose the less
// aggressive kLazy mode with a 60-second polling interval.
// For further results please see crbug.com/1116087.
PageLoadMetricsMemoryTracker::PageLoadMetricsMemoryTracker() {
  if (base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring)) {
    memory_request_ = std::make_unique<
        performance_manager::v8_memory::V8DetailedMemoryRequestAnySeq>(
        base::Seconds(60), performance_manager::v8_memory::
                               V8DetailedMemoryRequest::MeasurementMode::kLazy);
    memory_request_->AddObserver(this);
  }
}

PageLoadMetricsMemoryTracker::~PageLoadMetricsMemoryTracker() = default;

void PageLoadMetricsMemoryTracker::Shutdown() {
  if (memory_request_) {
    memory_request_->RemoveObserver(this);
    memory_request_.reset();
  }
}

void PageLoadMetricsMemoryTracker::OnV8MemoryMeasurementAvailable(
    performance_manager::RenderProcessHostId render_process_host_id,
    const performance_manager::v8_memory::V8DetailedMemoryProcessData&
        process_data,
    const performance_manager::v8_memory::V8DetailedMemoryObserverAnySeq::
        FrameDataMap& frame_data) {
  std::map<MetricsWebContentsObserver*, ObserverWeakPtrAndMemoryUpdates>
      memory_update_map;

  // Iterate through frames with available measurements.
  for (const auto& map_pair : frame_data) {
    content::GlobalRenderFrameHostId frame_routing_id = map_pair.first;
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(frame_routing_id);

    // We lose a small amount of data due to a RenderFrameHost
    // sometimes no longer being alive by the time that a report is received.
    // UMA suggests we miss about 0.078% of updates on desktop and about 0.11%
    // on mobile (as measured 10/30/2020).
    // See crbug.com/1116087.
    if (!rfh)
      continue;

    int64_t delta_bytes =
        UpdateMemoryUsageAndGetDelta(rfh, map_pair.second.v8_bytes_used());

    // Only send updates that are nontrivial.
    if (delta_bytes == 0)
      continue;

    // Note that at this point, we are guaranteed that the frame is alive, and
    // frames cannot exist without an owning WebContents.
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    MetricsWebContentsObserver* observer =
        MetricsWebContentsObserver::FromWebContents(web_contents);

    if (!observer)
      continue;

    auto emplace_pair = memory_update_map.emplace(std::make_pair(
        observer, ObserverWeakPtrAndMemoryUpdates(
                      observer->AsWeakPtr(),
                      MemoryUpdate(rfh->GetGlobalId(), delta_bytes))));

    if (!emplace_pair.second) {
      emplace_pair.first->second.updates.emplace_back(
          MemoryUpdate(rfh->GetGlobalId(), delta_bytes));
    }
  }

  // Dispatch memory updates to each observer. Note that we store references to
  // MetricsWebContentsObservers as weakptrs. This is done to ensure that if a
  // WebContents was torn down synchronously as the result of a memory update
  // in a different WebContents, we would not have a dangling pointer.
  for (const auto& map_pair : memory_update_map) {
    MetricsWebContentsObserver* observer = map_pair.second.weak_ptr.get();

    if (!observer)
      continue;

    observer->OnV8MemoryChanged(map_pair.second.updates);
  }
}

void PageLoadMetricsMemoryTracker::OnRenderFrameDeleted(
    content::RenderFrameHost* render_frame_host,
    MetricsWebContentsObserver* observer) {
  DCHECK(render_frame_host);
  DCHECK(observer);

  auto it = per_frame_memory_usage_map_.find(render_frame_host->GetRoutingID());

  if (it == per_frame_memory_usage_map_.end())
    return;

  // The routing id for |render_frame_host| has been found in our usage map.
  // We assume that the renderer has released the frame and that its
  // contents will be picked up by the next GC. So for all intents and
  // purposes, the memory is freed at this point, and we remove the entry from
  // our usage map and notify observers of the delta.
  int64_t delta_bytes = -it->second;
  per_frame_memory_usage_map_.erase(it);

  // Only send updates that are nontrivial.
  if (delta_bytes == 0)
    return;

  std::vector<MemoryUpdate> update(
      {MemoryUpdate(render_frame_host->GetGlobalId(), delta_bytes)});
  observer->OnV8MemoryChanged(update);
}

int64_t PageLoadMetricsMemoryTracker::UpdateMemoryUsageAndGetDelta(
    content::RenderFrameHost* render_frame_host,
    uint64_t current_bytes_used) {
  DCHECK(render_frame_host);

  int64_t delta_bytes = current_bytes_used;
  int routing_id = render_frame_host->GetRoutingID();
  auto it = per_frame_memory_usage_map_.find(routing_id);

  if (it != per_frame_memory_usage_map_.end()) {
    delta_bytes -= it->second;
    it->second = current_bytes_used;
  } else {
    per_frame_memory_usage_map_[routing_id] = current_bytes_used;
  }

  return delta_bytes;
}

}  // namespace page_load_metrics
