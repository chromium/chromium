// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_metrics_debug_string.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

TEST(PageLoadMetricsDebugStringTest, SoftNavigationMetrics) {
  mojom::SoftNavigationMetrics soft_navigation_metrics;
  soft_navigation_metrics.count = 1;
  soft_navigation_metrics.start_time = base::Milliseconds(123);
  soft_navigation_metrics.navigation_id = 456;
  soft_navigation_metrics.largest_contentful_paint =
      mojom::LargestContentfulPaintTiming::New();
  soft_navigation_metrics.largest_contentful_paint->largest_image_paint =
      base::Milliseconds(789);
  soft_navigation_metrics.largest_contentful_paint->largest_image_paint_size =
      1024;

  EXPECT_EQ(DebugString(soft_navigation_metrics),
            "{count: 1, start_time: 123, navigation_id: 456, "
            "largest_contentful_paint: {largest_image_paint: 789, "
            "largest_image_paint_size: 1024, largest_text_paint_size: 0, "
            "type: 0, image_bpp: 0, image_request_priority_valid: 0, "
            "image_request_priority_value: THROTTLED}}");
}

TEST(PageLoadMetricsDebugStringTest, PageLoadTiming) {
  mojom::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromMillisecondsSinceUnixEpoch(1000);
  timing.response_start = base::Milliseconds(10);
  timing.document_timing = mojom::DocumentTiming::New();
  timing.document_timing->dom_content_loaded_event_start =
      base::Milliseconds(20);
  timing.paint_timing = mojom::PaintTiming::New();
  timing.paint_timing->first_paint = base::Milliseconds(30);

  auto bfcache_timing = mojom::BackForwardCacheTiming::New();
  bfcache_timing->first_paint_after_back_forward_cache_restore =
      base::Milliseconds(120);
  bfcache_timing->request_animation_frames_after_back_forward_cache_restore
      .emplace_back(base::Milliseconds(24));
  bfcache_timing->request_animation_frames_after_back_forward_cache_restore
      .emplace_back(base::Milliseconds(73));
  timing.back_forward_cache_timings.emplace_back(std::move(bfcache_timing));
  auto bfcache_timing2 = mojom::BackForwardCacheTiming::New();
  bfcache_timing2->first_paint_after_back_forward_cache_restore =
      base::Milliseconds(150);
  timing.back_forward_cache_timings.emplace_back(std::move(bfcache_timing2));

  EXPECT_EQ(DebugString(timing),
            "{navigation_start: 1000, "
            "response_start: 10, "
            "document_timing: {dom_content_loaded_event_start: 20}, "
            "paint_timing: {first_paint: 30}, "
            "back_forward_cache_timings: "
            "[{first_paint_after_back_forward_cache_restore: 120, "
            "request_animation_frames_after_back_forward_cache_restore: "
            "[24, 73]}, "
            "{first_paint_after_back_forward_cache_restore: 150}]}");
}

}  // namespace page_load_metrics
