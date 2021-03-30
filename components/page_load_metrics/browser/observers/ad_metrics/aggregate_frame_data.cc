// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/aggregate_frame_data.h"

#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace page_load_metrics {

AggregateFrameData::AggregateFrameData() = default;
AggregateFrameData::~AggregateFrameData() = default;

void AggregateFrameData::UpdateCpuUsage(base::TimeTicks update_time,
                                        base::TimeDelta update,
                                        bool is_ad) {
  // Update the overall usage for all of the relevant buckets.
  cpu_usage_ += update;

  // Update the peak usage.
  total_peak_cpu_.UpdatePeakWindowedPercent(update, update_time);
  if (!is_ad)
    non_ad_peak_cpu_.UpdatePeakWindowedPercent(update, update_time);
}

void AggregateFrameData::ProcessResourceLoadInFrame(
    const mojom::ResourceDataUpdatePtr& resource,
    bool is_main_frame) {
  resource_data_.ProcessResourceLoad(resource);
  if (is_main_frame)
    main_frame_resource_data_.ProcessResourceLoad(resource);
}

void AggregateFrameData::AdjustAdBytes(int64_t unaccounted_ad_bytes,
                                       ResourceMimeType mime_type,
                                       bool is_main_frame) {
  resource_data_.AdjustAdBytes(unaccounted_ad_bytes, mime_type);
  if (is_main_frame)
    main_frame_resource_data_.AdjustAdBytes(unaccounted_ad_bytes, mime_type);
}

}  // namespace page_load_metrics
