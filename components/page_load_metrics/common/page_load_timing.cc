// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_timing.h"

namespace page_load_metrics {

mojom::PageLoadTimingPtr CreatePageLoadTiming() {
  return mojom::PageLoadTiming::New(
      base::Time(), absl::optional<base::TimeDelta>(),
      mojom::DocumentTiming::New(), mojom::InteractiveTiming::New(),
      mojom::PaintTiming::New(absl::nullopt, absl::nullopt, absl::nullopt,
                              absl::nullopt,
                              mojom::LargestContentfulPaintTiming::New(),
                              mojom::LargestContentfulPaintTiming::New(),
                              absl::nullopt, absl::nullopt, absl::nullopt),
      mojom::ParseTiming::New(),
      std::vector<mojo::StructPtr<mojom::BackForwardCacheTiming>>{},
      absl::optional<base::TimeDelta>(), absl::optional<base::TimeDelta>(),
      absl::optional<base::TimeDelta>(), absl::optional<base::TimeDelta>(),
      absl::optional<base::TimeDelta>());
}

mojom::LargestContentfulPaintTimingPtr CreateLargestContentfulPaintTiming() {
  return mojom::LargestContentfulPaintTiming::New();
}

bool IsEmpty(const page_load_metrics::mojom::DocumentTiming& timing) {
  return !timing.dom_content_loaded_event_start && !timing.load_event_start;
}

bool IsEmpty(const page_load_metrics::mojom::InteractiveTiming& timing) {
  return !timing.first_input_delay && !timing.first_input_timestamp &&
         !timing.longest_input_delay && !timing.longest_input_timestamp &&
         !timing.first_scroll_delay && !timing.first_scroll_timestamp &&
         !timing.first_input_processing_time;
}

bool IsEmpty(const page_load_metrics::mojom::InputTiming& timing) {
  return !timing.total_input_delay.InMilliseconds() &&
         !timing.total_adjusted_input_delay.InMilliseconds() &&
         !timing.num_input_events;
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

bool IsEmpty(const page_load_metrics::mojom::PageLoadTiming& timing) {
  return timing.navigation_start.is_null() && !timing.response_start &&
         (!timing.document_timing ||
          page_load_metrics::IsEmpty(*timing.document_timing)) &&
         (!timing.interactive_timing ||
          page_load_metrics::IsEmpty(*timing.interactive_timing)) &&
         (!timing.paint_timing ||
          page_load_metrics::IsEmpty(*timing.paint_timing)) &&
         (!timing.parse_timing ||
          page_load_metrics::IsEmpty(*timing.parse_timing)) &&
         timing.back_forward_cache_timings.empty() &&
         !timing.user_timing_mark_fully_loaded &&
         !timing.user_timing_mark_fully_visible &&
         !timing.user_timing_mark_interactive;
}

void InitPageLoadTimingForTest(mojom::PageLoadTiming* timing) {
  timing->document_timing = mojom::DocumentTiming::New();
  timing->interactive_timing = mojom::InteractiveTiming::New();
  timing->paint_timing = mojom::PaintTiming::New();
  timing->paint_timing->largest_contentful_paint =
      mojom::LargestContentfulPaintTiming::New();
  timing->paint_timing->experimental_largest_contentful_paint =
      mojom::LargestContentfulPaintTiming::New();
  timing->parse_timing = mojom::ParseTiming::New();
  timing->back_forward_cache_timings.clear();
}

}  // namespace page_load_metrics
