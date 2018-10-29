// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"

namespace data_reduction_proxy {

DataReductionProxyPageLoadTiming::DataReductionProxyPageLoadTiming(
    const base::Time& navigation_start,
    const base::Optional<base::TimeDelta>& response_start,
    const base::Optional<base::TimeDelta>& load_event_start,
    const base::Optional<base::TimeDelta>& first_image_paint,
    const base::Optional<base::TimeDelta>& first_contentful_paint,
    const base::Optional<base::TimeDelta>& experimental_first_meaningful_paint,
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
    uint32_t scroll_count)
    : navigation_start(navigation_start),
      response_start(response_start),
      load_event_start(load_event_start),
      first_image_paint(first_image_paint),
      first_contentful_paint(first_contentful_paint),
      experimental_first_meaningful_paint(experimental_first_meaningful_paint),
      first_input_delay(first_input_delay),
      parse_blocked_on_script_load_duration(
          parse_blocked_on_script_load_duration),
      parse_stop(parse_stop),
      page_end_time(page_end_time),
      network_bytes(network_bytes),
      original_network_bytes(original_network_bytes),
      total_page_size_bytes(total_page_size_bytes),
      cached_fraction(cached_fraction),
      app_background_occurred(app_background_occurred),
      opt_out_occurred(opt_out_occurred),
      renderer_memory_usage_kb(renderer_memory_usage_kb),
      host_id(host_id),
      page_end_reason(page_end_reason),
      touch_count(touch_count),
      scroll_count(scroll_count) {}

DataReductionProxyPageLoadTiming::DataReductionProxyPageLoadTiming(
    const DataReductionProxyPageLoadTiming& other) = default;

}  // namespace data_reduction_proxy
