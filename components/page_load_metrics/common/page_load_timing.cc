// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "third_party/blink/public/common/performance/performance_timeline_constants.h"

namespace page_load_metrics {

mojom::PageLoadTimingPtr CreatePageLoadTiming() {
  return mojom::PageLoadTiming::New(
      base::Time(), std::optional<base::TimeDelta>(),
      std::optional<base::TimeDelta>(), mojom::DocumentTiming::New(),
      mojom::InteractiveTiming::New(),
      mojom::PaintTiming::New(
          std::nullopt, std::nullopt, std::nullopt, std::nullopt,
          CreateLargestContentfulPaintTiming(),
          CreateLargestContentfulPaintTiming(), std::nullopt, std::nullopt),
      mojom::ParseTiming::New(), mojom::DomainLookupTiming::New(),
      std::vector<mojo::StructPtr<mojom::BackForwardCacheTiming>>{},
      std::optional<base::TimeDelta>(), std::optional<base::TimeDelta>(),
      std::optional<base::TimeDelta>(), std::optional<base::TimeDelta>(),
      std::optional<base::TimeDelta>());
}

mojom::LargestContentfulPaintTimingPtr CreateLargestContentfulPaintTiming() {
  auto timing = mojom::LargestContentfulPaintTiming::New();
  timing->resource_load_timings = mojom::LcpResourceLoadTimings::New();
  return timing;
}

mojom::SoftNavigationMetricsPtr CreateSoftNavigationMetrics() {
  return mojom::SoftNavigationMetrics::New(
      blink::kSoftNavigationCountDefaultValue, base::Milliseconds(0),
      std::string(), CreateLargestContentfulPaintTiming());
}

bool IsEmpty(const page_load_metrics::mojom::DocumentTiming& timing) {
  return !timing.dom_content_loaded_event_start && !timing.load_event_start;
}

bool IsEmpty(const page_load_metrics::mojom::InteractiveTiming& timing) {
  return !timing.first_input_delay && !timing.first_input_timestamp &&
         !timing.first_scroll_delay && !timing.first_scroll_timestamp;
}

bool IsEmpty(const page_load_metrics::mojom::InputTiming& timing) {
  // TODO(sullivan): Adjust this to be based on max_event_durations
  return !timing.num_interactions;
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

bool IsEmpty(const page_load_metrics::mojom::PageLoadTiming& timing) {
  return timing.navigation_start.is_null() && !timing.connect_start &&
         !timing.response_start &&
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
         !timing.user_timing_mark_interactive;
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
  timing->back_forward_cache_timings.clear();
}

}  // namespace page_load_metrics
