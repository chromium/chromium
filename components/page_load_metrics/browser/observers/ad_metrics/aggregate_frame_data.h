// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AGGREGATE_FRAME_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AGGREGATE_FRAME_DATA_H_

#include <stdint.h>

#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"

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

  int peak_windowed_non_ad_cpu_percent() const {
    return non_ad_peak_cpu_.peak_windowed_percent();
  }

  int peak_windowed_cpu_percent() const {
    return total_peak_cpu_.peak_windowed_percent();
  }

  // TODO(crbug.com/1136068): The size_t members should probably be int64_t.
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
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AGGREGATE_FRAME_DATA_H_
