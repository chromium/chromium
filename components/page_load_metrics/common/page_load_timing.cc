// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_timing.h"

#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"

namespace page_load_metrics {

mojom::PageLoadTimingPtr CreatePageLoadTiming() {
  return mojom::PageLoadTiming::New(
      base::Time(), std::optional<base::TimeDelta>(),
      std::optional<base::TimeDelta>(), std::optional<base::TimeDelta>(),
      mojom::DocumentTiming::New(), mojom::InteractiveTiming::New(),
      mojom::PaintTiming::New(
          std::nullopt, std::nullopt, std::nullopt, std::nullopt,
          CreateLargestContentfulPaintTiming(),
          CreateLargestContentfulPaintTiming(), std::nullopt, std::nullopt),
      mojom::ParseTiming::New(), mojom::DomainLookupTiming::New(),
      std::vector<mojo::StructPtr<mojom::BackForwardCacheTiming>>{},
      std::optional<base::TimeDelta>(), std::optional<base::TimeDelta>(),
      std::optional<base::TimeDelta>(), std::optional<base::TimeDelta>(),
      std::optional<base::TimeDelta>(), /*monotonic_paint_timing=*/nullptr);
}

mojom::LargestContentfulPaintTimingPtr CreateLargestContentfulPaintTiming() {
  auto timing = mojom::LargestContentfulPaintTiming::New();
  timing->resource_load_timings = mojom::LcpResourceLoadTimings::New();
  return timing;
}

mojom::SoftNavigationMetricsPtr CreateSoftNavigationMetrics() {
  auto timing = mojom::SoftNavigationMetrics::New();
  timing->navigation_id = 0;
  timing->start_time = base::Milliseconds(0);
  timing->largest_contentful_paint = CreateLargestContentfulPaintTiming();
  return timing;
}

bool IsEmpty(const page_load_metrics::mojom::DocumentTiming& timing) {
  return !timing.dom_content_loaded_event_start && !timing.load_event_start;
}

bool IsEmpty(const page_load_metrics::mojom::InteractiveTiming& timing) {
  return !timing.first_input_delay && !timing.first_input_timestamp &&
         !timing.first_scroll_delay && !timing.first_scroll_timestamp;
}

bool IsEmpty(const page_load_metrics::mojom::InputTiming& timing) {
  return timing.user_interaction_latencies.empty();
}

bool IsEmpty(const page_load_metrics::mojom::PaintTiming& timing) {
  return !timing.first_paint && !timing.first_image_paint &&
         !timing.first_contentful_paint && !timing.first_meaningful_paint &&
         !timing.largest_contentful_paint->largest_image_paint &&
         !timing.largest_contentful_paint->largest_text_paint &&
         !timing.experimental_largest_contentful_paint->largest_image_paint &&
         !timing.experimental_largest_contentful_paint->largest_text_paint &&
         !timing.first_eligible_to_paint;
}

bool IsEmpty(const page_load_metrics::mojom::ParseTiming& timing) {
  return !timing.parse_start && !timing.parse_stop &&
         !timing.parse_blocked_on_script_load_duration &&
         !timing.parse_blocked_on_script_load_from_document_write_duration &&
         !timing.parse_blocked_on_script_execution_duration &&
         !timing.parse_blocked_on_script_execution_from_document_write_duration;
}

bool IsEmpty(const page_load_metrics::mojom::DomainLookupTiming& timing) {
  return !timing.domain_lookup_start && !timing.domain_lookup_end;
}

bool IsEmpty(const mojom::LcpResourceLoadTimings& timing) {
  return !timing.discovery_time && !timing.load_start && !timing.load_end;
}

bool IsEmpty(const mojom::LargestContentfulPaintTiming& timing) {
  return !timing.largest_image_paint && !timing.largest_text_paint &&
         (!timing.resource_load_timings ||
          IsEmpty(*timing.resource_load_timings));
}

bool IsEmpty(const mojom::MonotonicPaintTiming& timing) {
  return !timing.first_paint && !timing.first_contentful_paint;
}

bool IsEmpty(const mojom::SoftNavigationMetrics& timing) {
  return !timing.count && timing.start_time.is_zero() &&
         !timing.navigation_id && !timing.same_document_metrics_token &&
         (!timing.largest_contentful_paint ||
          IsEmpty(*timing.largest_contentful_paint));
}

bool IsEmpty(const page_load_metrics::mojom::PageLoadTiming& timing) {
  return timing.navigation_start.is_null() && !timing.connect_start &&
         !timing.connect_end && !timing.response_start &&
         (!timing.document_timing ||
          page_load_metrics::IsEmpty(*timing.document_timing)) &&
         (!timing.interactive_timing ||
          page_load_metrics::IsEmpty(*timing.interactive_timing)) &&
         (!timing.paint_timing ||
          page_load_metrics::IsEmpty(*timing.paint_timing)) &&
         (!timing.parse_timing ||
          page_load_metrics::IsEmpty(*timing.parse_timing)) &&
         (!timing.domain_lookup_timing ||
          page_load_metrics::IsEmpty(*timing.domain_lookup_timing)) &&
         timing.back_forward_cache_timings.empty() &&
         !timing.user_timing_mark_fully_loaded &&
         !timing.user_timing_mark_fully_visible &&
         !timing.user_timing_mark_interactive &&
         (!timing.monotonic_paint_timing ||
          IsEmpty(*timing.monotonic_paint_timing));
}

void InitPageLoadTimingForTest(mojom::PageLoadTiming* timing) {
  timing->document_timing = mojom::DocumentTiming::New();
  timing->domain_lookup_timing = mojom::DomainLookupTiming::New();
  timing->interactive_timing = mojom::InteractiveTiming::New();
  timing->paint_timing = mojom::PaintTiming::New();
  timing->paint_timing->largest_contentful_paint =
      CreateLargestContentfulPaintTiming();
  timing->paint_timing->experimental_largest_contentful_paint =
      CreateLargestContentfulPaintTiming();
  timing->parse_timing = mojom::ParseTiming::New();
  timing->monotonic_paint_timing.reset();
  timing->back_forward_cache_timings.clear();
}

}  // namespace page_load_metrics
