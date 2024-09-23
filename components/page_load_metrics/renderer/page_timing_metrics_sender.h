// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/renderer/page_resource_data_use.h"
#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature_tracker.h"
#include "third_party/blink/public/web/web_local_frame_client.h"

class GURL;

namespace base {
class OneShotTimer;
}  // namespace base

namespace blink {
struct JavaScriptFrameworkDetectionResult;
}  // namespace blink

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace blink {
struct SoftNavigationMetrics;
}  // namespace blink

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
                          std::unique_ptr<PageResourceDataUse> initial_request,
                          bool is_main_frame);

  PageTimingMetricsSender(const PageTimingMetricsSender&) = delete;
  PageTimingMetricsSender& operator=(const PageTimingMetricsSender&) = delete;

  ~PageTimingMetricsSender();

  void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior);
  void DidObserveJavaScriptFrameworks(
      const blink::JavaScriptFrameworkDetectionResult&);
  void DidObserveSubresourceLoad(
      const blink::SubresourceLoadMetrics& subresource_load_metrics);
  void DidObserveNewFeatureUsage(const blink::UseCounterFeature& feature);
  void DidObserveSoftNavigation(blink::SoftNavigationMetrics metrics);
  void DidObserveLayoutShift(double score, bool after_input_or_scroll);

  void DidStartResponse(const url::SchemeHostPort& final_response_url,
                        int resource_id,
                        const network::mojom::URLResponseHead& response_head,
                        network::mojom::RequestDestination request_destination,
                        bool is_ad_resource);
  void DidReceiveTransferSizeUpdate(int resource_id, int received_data_length);
  void DidCompleteResponse(int resource_id,
                           const network::URLLoaderCompletionStatus& status);
  void DidCancelResponse(int resource_id);
  void DidLoadResourceFromMemoryCache(const GURL& response_url,
                                      int request_id,
                                      int64_t encoded_body_length,
                                      const std::string& mime_type);
  void OnMainFrameIntersectionChanged(
      const gfx::Rect& main_frame_intersection_rect);
  void OnMainFrameViewportRectangleChanged(
      const gfx::Rect& main_frame_viewport_rect);
  void OnMainFrameImageAdRectangleChanged(int element_id,
                                          const gfx::Rect& image_ad_rect);

  void DidObserveUserInteraction(base::TimeTicks max_event_start,
                                 base::TimeTicks max_event_queued_main_thread,
                                 base::TimeTicks max_event_commit_finish,
                                 base::TimeTicks max_event_end,
                                 blink::UserInteractionType interaction_type,
                                 uint64_t interaction_offset);
  // Updates the timing information. Buffers |timing| to be sent over mojo
  // sometime 'soon'.
  void Update(
      mojom::PageLoadTimingPtr timing,
      const PageTimingMetadataRecorder::MonotonicTiming& monotonic_timing);

  // Sends any queued timing data immediately and stops the send timer.
  void SendLatest();

  // Updates the PageLoadMetrics::CpuTiming data and starts the send timer.
  void UpdateCpuTiming(base::TimeDelta task_time);

  void UpdateResourceMetadata(int resource_id, bool is_main_frame_resource);
  void SetUpSmoothnessReporting(base::ReadOnlySharedMemoryRegion shared_memory);
  void InitiateUserInteractionTiming();
  mojom::SoftNavigationMetricsPtr GetSoftNavigationMetrics() {
    return soft_navigation_metrics_->Clone();
  }

  void UpdateSoftNavigationMetrics(
      mojom::SoftNavigationMetricsPtr soft_navigation_metrics);

  void SendCustomUserTimingMark(mojom::CustomUserTimingMarkPtr custom_timing);

 protected:
  base::OneShotTimer* timer() const { return timer_.get(); }

 private:
  void EnsureSendTimer(bool urgent = false);
  void SendNow();

  // Inserts a `PageResourceDataUse` with `resource_id` in
  // `page_resource_data_use_` if none exists. Returns a pointer to the inserted
  // entry or to the existing one.
  PageResourceDataUse* FindOrInsertPageResourceDataUse(int resource_id);

  std::unique_ptr<PageTimingSender> sender_;
  std::unique_ptr<base::OneShotTimer> timer_;
  mojom::PageLoadTimingPtr last_timing_;
  mojom::CpuTimingPtr last_cpu_timing_;
  mojom::InputTimingPtr input_timing_delta_;
  std::optional<blink::SubresourceLoadMetrics> subresource_load_metrics_;

  // The the sender keep track of metadata as it comes in, because the sender is
  // scoped to a single committed load.
  mojom::FrameMetadataPtr metadata_;
  // A list of newly observed features during page load, to be sent to the
  // browser.
  std::vector<blink::UseCounterFeature> new_features_;
  mojom::FrameRenderDataUpdate render_data_;

  blink::UseCounterFeatureTracker feature_tracker_;

  mojom::SoftNavigationMetricsPtr soft_navigation_metrics_;

  bool have_sent_ipc_ = false;

  // The page's resources that are currently loading,  or were completed after
  // the last timing update.
  base::small_map<std::map<int, std::unique_ptr<PageResourceDataUse>>, 16>
      page_resource_data_use_;

  // Set of all resources that have completed or received a transfer
  // size update since the last timimg update.
  base::flat_set<raw_ptr<PageResourceDataUse, CtnExperimental>>
      modified_resources_;

  // Field trial for alternating page timing metrics sender buffer timer delay.
  // https://crbug.com/847269.
  int buffer_timer_delay_ms_;

  // Responsible for recording sampling profiler metadata corresponding to page
  // timing.
  PageTimingMetadataRecorder metadata_recorder_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
