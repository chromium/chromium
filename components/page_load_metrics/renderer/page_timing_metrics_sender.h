// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_

#include <bitset>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/renderer/page_resource_data_use.h"
#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom-shared.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/public/web/web_local_frame_client.h"

class GURL;

namespace base {
class OneShotTimer;
}  // namespace base

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace page_load_metrics {

class PageTimingSender;

// PageTimingMetricsSender is responsible for sending page load timing metrics
// over IPC. PageTimingMetricsSender may coalesce sent IPCs in order to
// minimize IPC contention.
class PageTimingMetricsSender {
 public:
  PageTimingMetricsSender(std::unique_ptr<PageTimingSender> sender,
                          std::unique_ptr<base::OneShotTimer> timer,
                          mojom::PageLoadTimingPtr initial_timing,
                          const PageTimingMetadataRecorder::MonotonicTiming&
                              initial_monotonic_timing,
                          std::unique_ptr<PageResourceDataUse> initial_request);
  ~PageTimingMetricsSender();

  void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior);
  void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature);
  void DidObserveNewCssPropertyUsage(blink::mojom::CSSSampleId css_property,
                                     bool is_animated);
  void DidObserveLayoutShift(double score, bool after_input_or_scroll);
  void DidObserveLayoutNg(uint32_t all_block_count,
                          uint32_t ng_block_count,
                          uint32_t all_call_count,
                          uint32_t ng_call_count);
  void DidObserveLazyLoadBehavior(
      blink::WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior);

  void DidStartResponse(const GURL& response_url,
                        int resource_id,
                        const network::mojom::URLResponseHead& response_head,
                        network::mojom::RequestDestination request_destination);
  void DidReceiveTransferSizeUpdate(int resource_id, int received_data_length);
  void DidCompleteResponse(int resource_id,
                           const network::URLLoaderCompletionStatus& status);
  void DidCancelResponse(int resource_id);
  void DidLoadResourceFromMemoryCache(const GURL& response_url,
                                      int request_id,
                                      int64_t encoded_body_length,
                                      const std::string& mime_type);
  void OnMainFrameIntersectionChanged(const blink::WebRect& intersect_rect);

  void DidObserveInputDelay(base::TimeDelta input_delay);
  // Updates the timing information. Buffers |timing| to be sent over mojo
  // sometime 'soon'.
  void Update(
      mojom::PageLoadTimingPtr timing,
      const PageTimingMetadataRecorder::MonotonicTiming& monotonic_timing);

  // Sends any queued timing data immediately and stops the send timer.
  void SendLatest();

  // Updates the PageLoadMetrics::CpuTiming data and starts the send timer.
  void UpdateCpuTiming(base::TimeDelta task_time);

  void UpdateResourceMetadata(int resource_id,
                              bool is_ad_resource,
                              bool is_main_frame_resource,
                              bool completed_before_fcp);
  void SetUpSmoothnessReporting(base::ReadOnlySharedMemoryRegion shared_memory);

 protected:
  base::OneShotTimer* timer() const { return timer_.get(); }

 private:
  void EnsureSendTimer();
  void SendNow();
  void ClearNewFeatures();

  std::unique_ptr<PageTimingSender> sender_;
  std::unique_ptr<base::OneShotTimer> timer_;
  mojom::PageLoadTimingPtr last_timing_;
  mojom::CpuTimingPtr last_cpu_timing_;
  mojom::InputTimingPtr input_timing_delta_;

  // The the sender keep track of metadata as it comes in, because the sender is
  // scoped to a single committed load.
  mojom::FrameMetadataPtr metadata_;
  // A list of newly observed features during page load, to be sent to the
  // browser.
  mojom::PageLoadFeaturesPtr new_features_;
  mojom::FrameRenderDataUpdate render_data_;
  mojom::DeferredResourceCountsPtr new_deferred_resource_data_;

  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      features_sent_;
  std::bitset<static_cast<size_t>(blink::mojom::CSSSampleId::kMaxValue) + 1>
      css_properties_sent_;
  std::bitset<static_cast<size_t>(blink::mojom::CSSSampleId::kMaxValue) + 1>
      animated_css_properties_sent_;

  bool have_sent_ipc_ = false;

  // The page's resources that are currently loading,  or were completed after
  // the last timing update.
  base::small_map<std::map<int, std::unique_ptr<PageResourceDataUse>>, 16>
      page_resource_data_use_;

  // Set of all resources that have completed or received a transfer
  // size update since the last timimg update.
  base::flat_set<PageResourceDataUse*> modified_resources_;

  // Field trial for alternating page timing metrics sender buffer timer delay.
  // https://crbug.com/847269.
  int buffer_timer_delay_ms_;

  // Responsible for recording sampling profiler metadata corresponding to page
  // timing.
  PageTimingMetadataRecorder metadata_recorder_;

  DISALLOW_COPY_AND_ASSIGN(PageTimingMetricsSender);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
