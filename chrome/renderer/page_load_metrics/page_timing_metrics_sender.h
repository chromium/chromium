// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_TIMING_METRICS_SENDER_H_
#define CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_TIMING_METRICS_SENDER_H_

#include <bitset>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "chrome/renderer/page_load_metrics/page_resource_data_use.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom.h"
#include "third_party/blink/public/platform/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace network {
struct ResourceResponseHead;
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
                          std::unique_ptr<PageResourceDataUse> initial_request);
  ~PageTimingMetricsSender();

  void DidObserveLoadingBehavior(blink::WebLoadingBehaviorFlag behavior);
  void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature);
  void DidObserveNewCssPropertyUsage(int css_property, bool is_animated);
  void DidObserveLayoutJank(double jank_fraction);
  void DidStartResponse(int resource_id,
                        const network::ResourceResponseHead& response_head);
  void DidReceiveTransferSizeUpdate(int resource_id, int received_data_length);
  void DidCompleteResponse(int resource_id,
                           const network::URLLoaderCompletionStatus& status);
  void DidCancelResponse(int resource_id);

  void Send(mojom::PageLoadTimingPtr timing);

  void UpdateResourceMetadata(int resource_id,
                              bool is_ad_resource,
                              bool is_main_frame_resource);

 protected:
  base::OneShotTimer* timer() const { return timer_.get(); }

 private:
  void EnsureSendTimer();
  void SendNow();
  void ClearNewFeatures();

  std::unique_ptr<PageTimingSender> sender_;
  std::unique_ptr<base::OneShotTimer> timer_;
  mojom::PageLoadTimingPtr last_timing_;

  // The the sender keep track of metadata as it comes in, because the sender is
  // scoped to a single committed load.
  mojom::PageLoadMetadataPtr metadata_;
  // A list of newly observed features during page load, to be sent to the
  // browser.
  mojom::PageLoadFeaturesPtr new_features_;
  mojom::PageRenderData render_data_;

  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      features_sent_;
  std::bitset<static_cast<size_t>(blink::mojom::kMaximumCSSSampleId + 1)>
      css_properties_sent_;
  std::bitset<static_cast<size_t>(blink::mojom::kMaximumCSSSampleId + 1)>
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

  DISALLOW_COPY_AND_ASSIGN(PageTimingMetricsSender);
};

}  // namespace page_load_metrics

#endif  // CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_TIMING_METRICS_SENDER_H_
