// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_timing.h"

namespace page_load_metrics {

mojom::PageLoadTimingPtr CreatePageLoadTiming() {
  return mojom::PageLoadTiming::New(
      base::Time(), base::Optional<base::TimeDelta>(),
      mojom::DocumentTiming::New(), mojom::InteractiveTiming::New(),
      mojom::PaintTiming::New(), mojom::ParseTiming::New(),
      base::Optional<base::TimeDelta>());
}

bool IsEmpty(const page_load_metrics::mojom::DocumentTiming& timing) {
  return !timing.dom_content_loaded_event_start && !timing.load_event_start &&
         !timing.first_layout;
}

bool IsEmpty(const page_load_metrics::mojom::InteractiveTiming& timing) {
  return !timing.interactive && !timing.interactive_detection &&
         !timing.first_invalidating_input && !timing.first_input_delay &&
         !timing.first_input_timestamp && !timing.longest_input_delay &&
         !timing.longest_input_timestamp;
}
bool IsEmpty(const page_load_metrics::mojom::PaintTiming& timing) {
  return !timing.first_paint && !timing.first_image_paint &&
         !timing.first_contentful_paint && !timing.first_meaningful_paint &&
         !timing.largest_image_paint && !timing.largest_text_paint;
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
          page_load_metrics::IsEmpty(*timing.parse_timing));
}

void InitPageLoadTimingForTest(mojom::PageLoadTiming* timing) {
  timing->document_timing = mojom::DocumentTiming::New();
  timing->interactive_timing = mojom::InteractiveTiming::New();
  timing->paint_timing = mojom::PaintTiming::New();
  timing->parse_timing = mojom::ParseTiming::New();
}

}  // namespace page_load_metrics
