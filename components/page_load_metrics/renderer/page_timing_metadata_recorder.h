// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METADATA_RECORDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METADATA_RECORDER_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace page_load_metrics {

// Records metadata corresponding to page load metrics on sampling profiler
// stack samples. PageTimingMetadataRecorder is currently only intended to be
// used for the sampling profiler. If you have a new use case in mind, please
// reach out to page_load_metrics owners to discuss it.
class PageTimingMetadataRecorder {
 public:
  // Records the monotonic times that define first contentful paint.
  struct MonotonicTiming {
    MonotonicTiming();

    MonotonicTiming(const MonotonicTiming&);
    MonotonicTiming& operator=(const MonotonicTiming&);
    MonotonicTiming(MonotonicTiming&&);
    MonotonicTiming& operator=(MonotonicTiming&&);

    absl::optional<base::TimeTicks> navigation_start;
    absl::optional<base::TimeTicks> first_contentful_paint;
  };

  PageTimingMetadataRecorder(const MonotonicTiming& initial_timing);
  ~PageTimingMetadataRecorder();

  PageTimingMetadataRecorder(const PageTimingMetadataRecorder&) = delete;
  PageTimingMetadataRecorder& operator=(const PageTimingMetadataRecorder&) =
      delete;

  void UpdateMetadata(const MonotonicTiming& timing);

 private:
  // Uniquely identifies an instance of the PageTimingMetadataRecorder. Used to
  // distinguish page loads for different documents when applying sample
  // metadata.
  const int instance_id_;

  // Records the monotonic times that define first contentful paint.
  MonotonicTiming timing_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METADATA_RECORDER_H_
