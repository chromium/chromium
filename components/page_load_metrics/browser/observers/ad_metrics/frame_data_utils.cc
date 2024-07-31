// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace page_load_metrics {

ResourceLoadAggregator::ResourceLoadAggregator() = default;
ResourceLoadAggregator::~ResourceLoadAggregator() = default;

// static
ResourceMimeType ResourceLoadAggregator::GetResourceMimeType(
    const mojom::ResourceDataUpdatePtr& resource) {
  if (blink::IsSupportedImageMimeType(resource->mime_type))
    return ResourceMimeType::kImage;
  if (blink::IsSupportedJavascriptMimeType(resource->mime_type))
    return ResourceMimeType::kJavascript;

  std::string top_level_type;
  std::string subtype;
  // Categorize invalid mime types as "Other".
  if (!net::ParseMimeTypeWithoutParameter(resource->mime_type, &top_level_type,
                                          &subtype)) {
    return ResourceMimeType::kOther;
  }
  if (top_level_type.compare("video") == 0)
    return ResourceMimeType::kVideo;
  if (top_level_type.compare("text") == 0 && subtype.compare("css") == 0)
    return ResourceMimeType::kCss;
  if (top_level_type.compare("text") == 0 && subtype.compare("html") == 0)
    return ResourceMimeType::kHtml;
  return ResourceMimeType::kOther;
}

void ResourceLoadAggregator::ProcessResourceLoad(
    const mojom::ResourceDataUpdatePtr& resource) {
  bytes_ += resource->delta_bytes;
  network_bytes_ += resource->delta_bytes;

  // Report cached resource body bytes to overall frame bytes.
  if (resource->is_complete &&
      resource->cache_type != mojom::CacheType::kNotCached) {
    bytes_ += resource->encoded_body_length;
  }

  if (resource->reported_as_ad_resource) {
    ad_network_bytes_ += resource->delta_bytes;
    ad_bytes_ += resource->delta_bytes;
    // Report cached resource body bytes to overall frame bytes.
    if (resource->is_complete &&
        resource->cache_type != mojom::CacheType::kNotCached)
      ad_bytes_ += resource->encoded_body_length;

    ResourceMimeType mime_type = GetResourceMimeType(resource);
    ad_bytes_by_mime_[static_cast<size_t>(mime_type)] += resource->delta_bytes;
  }
}

void ResourceLoadAggregator::AdjustAdBytes(int64_t unaccounted_ad_bytes,
                                           ResourceMimeType mime_type) {
  ad_network_bytes_ += unaccounted_ad_bytes;
  ad_bytes_ += unaccounted_ad_bytes;
  ad_bytes_by_mime_[static_cast<size_t>(mime_type)] += unaccounted_ad_bytes;
}

PeakCpuAggregator::PeakCpuAggregator() = default;
PeakCpuAggregator::~PeakCpuAggregator() = default;

// static
constexpr base::TimeDelta PeakCpuAggregator::kWindowSize;

void PeakCpuAggregator::UpdatePeakWindowedPercent(
    base::TimeDelta cpu_usage_update,
    base::TimeTicks update_time) {
  current_window_total_ += cpu_usage_update;
  current_window_updates_.push(CpuUpdateData(update_time, cpu_usage_update));
  base::TimeTicks cutoff_time = update_time - kWindowSize;
  while (!current_window_updates_.empty() &&
         current_window_updates_.front().update_time < cutoff_time) {
    current_window_total_ -= current_window_updates_.front().usage_info;
    current_window_updates_.pop();
  }
  int current_windowed_percent = 100 * current_window_total_.InMilliseconds() /
                                 kWindowSize.InMilliseconds();
  if (current_windowed_percent > peak_windowed_percent_)
    peak_windowed_percent_ = current_windowed_percent;
}

void MemoryUsageAggregator::UpdateUsage(int64_t delta_bytes) {
  current_bytes_used_ += delta_bytes;
  if (current_bytes_used_ > max_bytes_used_)
    max_bytes_used_ = current_bytes_used_;
}

}  // namespace page_load_metrics
