// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AGGREGATE_FRAME_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AGGREGATE_FRAME_DATA_H_

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "content/public/browser/auction_result.h"

namespace page_load_metrics {

// AggregateFrameData stores information in aggregate for all the frames during
// a navigation.  It contains specific information on various types of frames
// and their usage, such as ad vs. non-ad frames, as well as information about
// usage across the navigation as a whole.
class AggregateFrameData {
 public:
  AggregateFrameData();
  ~AggregateFrameData();

  void ProcessResourceLoadInFrame(const mojom::ResourceDataUpdatePtr& resource,
                                  bool is_outermost_main_frame);

  // Adjusts the overall page and potentially main frame ad bytes.
  void AdjustAdBytes(int64_t unaccounted_ad_bytes,
                     ResourceMimeType mime_type,
                     bool is_outermost_main_frame);

  // Updates the cpu usage for the page, given whether update is for an ad.
  void UpdateCpuUsage(base::TimeTicks update_time,
                      base::TimeDelta update,
                      bool is_ad);

  // Called for each new ad frame FCP calculation, this method keeps track of
  // the earliest FCP after main frame nav start.
  void UpdateFirstAdFCPSinceNavStart(base::TimeDelta time_since_nav_start);

  // Called when a Fledge auction completes, this method tracks
  // if an auction completes before the `first_ad_fcp_after_main_nav_start()`.
  void OnAdAuctionComplete(bool is_server_auction,
                           bool is_on_device_auction,
                           content::AuctionResult result);

  std::optional<base::TimeDelta> first_ad_fcp_after_main_nav_start() const {
    return first_ad_fcp_after_main_nav_start_;
  }

  bool completed_fledge_server_auction_before_fcp() const {
    return completed_fledge_server_auction_before_fcp_;
  }

  bool completed_fledge_on_device_auction_before_fcp() const {
    return completed_fledge_on_device_auction_before_fcp_;
  }

  bool completed_only_winning_fledge_auctions() const {
    return completed_only_winning_fledge_auctions_;
  }

  int peak_windowed_non_ad_cpu_percent() const {
    return non_ad_peak_cpu_.peak_windowed_percent();
  }

  int peak_windowed_cpu_percent() const {
    return total_peak_cpu_.peak_windowed_percent();
  }

  // TODO(crbug.com/40152120): The size_t members should probably be int64_t.
  struct AdDataByVisibility {
    // The following are aggregated when metrics are recorded on navigation.
    size_t bytes = 0;
    size_t network_bytes = 0;
    size_t frames = 0;
    // MemoryUsage is aggregated when a memory update is received.
    MemoryUsageAggregator memory;
  };

  // Returns the appropriate AdDataByVisibility given the |visibility|.
  const AdDataByVisibility& get_ad_data_by_visibility(
      FrameVisibility visibility) {
    return ad_data_[static_cast<size_t>(visibility)];
  }

  // These functions update the various members of AdDataByVisibility given the
  // visibility.  They all increment the current value.
  void update_ad_bytes_by_visibility(FrameVisibility visibility, size_t bytes) {
    ad_data_[static_cast<size_t>(visibility)].bytes += bytes;
  }
  void update_ad_network_bytes_by_visibility(FrameVisibility visibility,
                                             size_t network_bytes) {
    ad_data_[static_cast<size_t>(visibility)].network_bytes += network_bytes;
  }
  void update_ad_frames_by_visibility(FrameVisibility visibility,
                                      size_t frames) {
    ad_data_[static_cast<size_t>(visibility)].frames += frames;
  }
  void update_ad_memory_by_visibility(FrameVisibility visibility,
                                      int64_t delta_bytes) {
    ad_data_[static_cast<size_t>(visibility)].memory.UpdateUsage(delta_bytes);
  }

  // Updates the memory for the main frame of the page.
  void update_outermost_main_frame_memory(int64_t delta_memory) {
    outermost_main_frame_memory_.UpdateUsage(delta_memory);
  }

  // Updates the total ad cpu usage for the page.
  void update_ad_cpu_usage(base::TimeDelta usage) { ad_cpu_usage_ += usage; }

  // Get the total memory usage for this page.
  int64_t outermost_main_frame_max_memory() const {
    return outermost_main_frame_memory_.max_bytes_used();
  }

  // Get the total cpu usage of this page.
  base::TimeDelta total_cpu_usage() const { return cpu_usage_; }
  base::TimeDelta total_ad_cpu_usage() const { return ad_cpu_usage_; }

  // Accessor for the total resource data of the page.
  const ResourceLoadAggregator& resource_data() const { return resource_data_; }
  const ResourceLoadAggregator& outermost_main_frame_resource_data() const {
    return outermost_main_frame_resource_data_;
  }

 private:
  // Stores the data for ads on a page according to visibility.
  AdDataByVisibility
      ad_data_[static_cast<size_t>(FrameVisibility::kMaxValue) + 1] = {};

  // The overall cpu usage for this page.
  base::TimeDelta cpu_usage_ = base::TimeDelta();
  base::TimeDelta ad_cpu_usage_ = base::TimeDelta();

  // The memory used by the outermost main frame.
  MemoryUsageAggregator outermost_main_frame_memory_;

  // The resource data for this page.
  ResourceLoadAggregator resource_data_;
  ResourceLoadAggregator outermost_main_frame_resource_data_;

  // The peak cpu usages for this page.
  PeakCpuAggregator total_peak_cpu_;
  PeakCpuAggregator non_ad_peak_cpu_;

  // The first FCP of any ad frame on the page.
  std::optional<base::TimeDelta> first_ad_fcp_after_main_nav_start_;

  // Whether an ad auction completed (without being aborted) before the first ad
  // FCP.
  bool completed_fledge_server_auction_before_fcp_ = false;
  bool completed_fledge_on_device_auction_before_fcp_ = false;
  // If only winning auctions completed before the first ad FCP. Aborted
  // auctions do not count as completed.
  bool completed_only_winning_fledge_auctions_ = true;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AGGREGATE_FRAME_DATA_H_
