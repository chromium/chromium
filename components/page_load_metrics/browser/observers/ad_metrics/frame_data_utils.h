// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_FRAME_DATA_UTILS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_FRAME_DATA_UTILS_H_

#include <stdint.h>
#include <string.h>

#include <array>

#include "base/byte_count.h"
#include "base/containers/queue.h"
#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"

namespace page_load_metrics {

// Whether or not the ad frame has a display: none styling.
enum FrameVisibility {
  kNonVisible = 0,
  kVisible = 1,
  kAnyVisibility = 2,
  kMaxValue = kAnyVisibility,
};

// High level categories of mime types for resources loaded by the frame.
enum class ResourceMimeType {
  kJavascript = 0,
  kVideo = 1,
  kImage = 2,
  kCss = 3,
  kHtml = 4,
  kOther = 5,
  kMaxValue = kOther,
};

class ResourceLoadAggregator {
 public:
  ResourceLoadAggregator();
  ~ResourceLoadAggregator();

  // Updates the number of bytes loaded in the frame given a resource load.
  void ProcessResourceLoad(const mojom::ResourceDataUpdatePtr& resource);

  // Adds additional bytes to the ad resource byte counts. This
  // is used to notify the frame that some bytes were tagged as ad bytes after
  // they were loaded.
  void AdjustAdBytes(base::ByteCount unaccounted_ad_bytes,
                     ResourceMimeType mime_type);

  // Get the mime type of a resource. This only returns a subset of mime types,
  // grouped at a higher level. For example, all video mime types return the
  // same value.
  // TODO(crbug.com/40152120): This is used well out of the scope of the
  // AdsPageLoadMetricsObserver and should sit in a common directory.
  static ResourceMimeType GetResourceMimeType(
      const mojom::ResourceDataUpdatePtr& resource);

  // Accessors for the various data stored in the class.

  base::ByteCount bytes() const { return bytes_; }

  base::ByteCount network_bytes() const { return network_bytes_; }

  base::ByteCount ad_bytes() const { return ad_bytes_; }

  base::ByteCount ad_network_bytes() const { return ad_network_bytes_; }

  base::ByteCount GetAdNetworkBytesForMime(ResourceMimeType mime_type) const {
    return ad_bytes_by_mime_[static_cast<size_t>(mime_type)];
  }

 private:
  // Total bytes used to load resources in the frame, including headers.
  base::ByteCount bytes_;
  base::ByteCount network_bytes_;

  // Ad network bytes for different mime type resources loaded in the frame.
  std::array<base::ByteCount,
             static_cast<size_t>(ResourceMimeType::kMaxValue) + 1>
      ad_bytes_by_mime_;

  // Tracks the number of bytes that were used to load resources which were
  // detected to be ads inside of this frame. For ad frames, these counts should
  // match |frame_bytes| and |frame_network_bytes|.
  base::ByteCount ad_bytes_;
  base::ByteCount ad_network_bytes_;
};

class PeakCpuAggregator {
 public:
  // Standard constructor / destructor.
  PeakCpuAggregator();
  ~PeakCpuAggregator();

  // Window over which to consider cpu time spent in an ad_frame.
  static constexpr base::TimeDelta kWindowSize = base::Seconds(30);

  // Update the peak window variables given the current update and update time.
  void UpdatePeakWindowedPercent(base::TimeDelta cpu_usage_update,
                                 base::TimeTicks update_time);

  // Accessor for the peak percent.
  int peak_windowed_percent() const { return peak_windowed_percent_; }

 private:
  // Time updates for the frame with a timestamp indicating when they arrived.
  // Used for windowed cpu load reporting.
  struct CpuUpdateData {
    base::TimeTicks update_time;
    base::TimeDelta usage_info;
    CpuUpdateData(base::TimeTicks time, base::TimeDelta info)
        : update_time(time), usage_info(info) {}
  };

  // The cpu time spent in the current window.
  base::TimeDelta current_window_total_;

  // The cpu updates themselves that are still relevant for the time window.
  // Note: Since the window is 30 seconds and PageLoadMetrics updates arrive at
  // most every half second, this can never have more than 60 elements.
  base::queue<CpuUpdateData> current_window_updates_;

  // The peak windowed cpu load during the unactivated period.
  int peak_windowed_percent_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_FRAME_DATA_UTILS_H_
