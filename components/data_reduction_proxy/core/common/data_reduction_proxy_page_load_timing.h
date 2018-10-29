// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PAGE_LOAD_TIMING_H
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PAGE_LOAD_TIMING_H

#include <stdint.h>

#include "base/optional.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"

namespace data_reduction_proxy {

// The timing information that is relevant to the Pageload metrics pingback.
struct DataReductionProxyPageLoadTiming {
  DataReductionProxyPageLoadTiming(
      const base::Time& navigation_start,
      const base::Optional<base::TimeDelta>& response_start,
      const base::Optional<base::TimeDelta>& load_event_start,
      const base::Optional<base::TimeDelta>& first_image_paint,
      const base::Optional<base::TimeDelta>& first_contentful_paint,
      const base::Optional<base::TimeDelta>&
          experimental_first_meaningful_paint,
      const base::Optional<base::TimeDelta>& first_input_delay,
      const base::Optional<base::TimeDelta>&
          parse_blocked_on_script_load_duration,
      const base::Optional<base::TimeDelta>& parse_stop,
      const base::Optional<base::TimeDelta>& page_end_time,
      int64_t network_bytes,
      int64_t original_network_bytes,
      int64_t total_page_size_bytes,
      float cached_fraction,
      bool app_background_occurred,
      bool opt_out_occurred,
      int64_t renderer_memory_usage_kb,
      int host_id,
      PageloadMetrics_PageEndReason page_end_reason,
      uint32_t touch_count,
      uint32_t scroll_count);

  DataReductionProxyPageLoadTiming(
      const DataReductionProxyPageLoadTiming& other);

  // Time that the navigation for the associated page was initiated.
  const base::Time navigation_start;

  // All TimeDeltas are relative to navigation_start.

  // Time that the first byte of the response is received.
  const base::Optional<base::TimeDelta> response_start;
  // Time immediately before the load event is fired.
  const base::Optional<base::TimeDelta> load_event_start;
  // Time when the first image is painted.
  const base::Optional<base::TimeDelta> first_image_paint;
  // Time when the first contentful thing (image, text, etc.) is painted.
  const base::Optional<base::TimeDelta> first_contentful_paint;
  // (Experimental) Time when the page's primary content is painted.
  const base::Optional<base::TimeDelta> experimental_first_meaningful_paint;
  // The queuing delay for the first user input on the page.
  const base::Optional<base::TimeDelta> first_input_delay;
  // Time that parsing was blocked by loading script.
  const base::Optional<base::TimeDelta> parse_blocked_on_script_load_duration;
  // Time when parsing completed.
  const base::Optional<base::TimeDelta> parse_stop;
  // Time when the page was ended (navigated away, Chrome backgrounded, etc).
  const base::Optional<base::TimeDelta> page_end_time;

  // The number of bytes served over the network, not including headers.
  const int64_t network_bytes;
  // The number of bytes that would have been served over the network if the
  // user were not using data reduction proxy, not including headers.
  const int64_t original_network_bytes;
  // The total number of bytes loaded for the page content, including cache.
  const int64_t total_page_size_bytes;
  // The fraction of bytes that were served from the cache for this page load.
  const float cached_fraction;
  // True when android app background occurred during the page load lifetime.
  const bool app_background_occurred;
  // True when the user clicks "Show Original" on the Previews infobar.
  const bool opt_out_occurred;
  // Kilobytes used by the renderer related to this page load. 0 if memory usage
  // is unknown.
  const int64_t renderer_memory_usage_kb;
  // The host id of the renderer if there was a renderer crash.
  const int host_id;
  // The reason that the page load ends.
  const PageloadMetrics_PageEndReason page_end_reason;
  // The number of touch events that happened on the page.
  const uint32_t touch_count;
  // The number of scroll events that happened on the page.
  const uint32_t scroll_count;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PAGE_LOAD_TIMING_H
