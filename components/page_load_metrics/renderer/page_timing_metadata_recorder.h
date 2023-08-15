// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METADATA_RECORDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METADATA_RECORDER_H_

#include <cstdint>
#include "base/profiler/sample_metadata.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace page_load_metrics {

// Records metadata corresponding to page load metrics on sampling profiler
// stack samples. PageTimingMetadataRecorder is currently only intended to be
// used for the sampling profiler. If you have a new use case in mind, please
// reach out to page_load_metrics owners to discuss it.
class PageTimingMetadataRecorder {
 public:
  // Records the monotonic times that define
  // - First contentful paint
  // - First input delay
  struct MonotonicTiming {
    MonotonicTiming();

    MonotonicTiming(const MonotonicTiming&);
    MonotonicTiming& operator=(const MonotonicTiming&);
    MonotonicTiming(MonotonicTiming&&);
    MonotonicTiming& operator=(MonotonicTiming&&);

    absl::optional<base::TimeTicks> navigation_start;
    absl::optional<base::TimeTicks> first_contentful_paint;

    absl::optional<base::TimeTicks> first_input_timestamp;
    absl::optional<base::TimeDelta> first_input_delay;
  };

  PageTimingMetadataRecorder(const MonotonicTiming& initial_timing);
  ~PageTimingMetadataRecorder();

  PageTimingMetadataRecorder(const PageTimingMetadataRecorder&) = delete;
  PageTimingMetadataRecorder& operator=(const PageTimingMetadataRecorder&) =
      delete;

  // Updates the metadata on past samples based on given timing. Called whenever
  // `PageTimingMetricsSender::Update` is called.
  void UpdateMetadata(const MonotonicTiming& timing);

  // Adds interaction duration metadata to past samples for a user interaction
  // with the given start and end time.
  void AddInteractionDurationMetadata(const base::TimeTicks interaction_start,
                                      const base::TimeTicks interaction_end);

  // Packs the 32 bit instance_id and interaction_id into one 64 bit signed int
  // to fit the int64 key field of the Metadata API. Public for testing.
  static int64_t CreateInteractionDurationMetadataKey(
      const uint32_t instance_id,
      const uint32_t interaction_id);

 protected:
  // To be overridden by test class.
  virtual void ApplyMetadataToPastSamples(base::TimeTicks period_start,
                                          base::TimeTicks period_end,
                                          base::StringPiece name,
                                          int64_t key,
                                          int64_t value,
                                          base::SampleMetadataScope scope);

 private:
  void UpdateFirstInputDelayMetadata(
      const absl::optional<base::TimeTicks>& first_input_timestamp,
      const absl::optional<base::TimeDelta>& first_input_delay);
  void UpdateFirstContentfulPaintMetadata(
      const absl::optional<base::TimeTicks>& navigation_start,
      const absl::optional<base::TimeTicks>& first_contentful_paint);

  // Uniquely identifies an instance of the PageTimingMetadataRecorder. Used to
  // distinguish page loads for different documents when applying sample
  // metadata.
  const uint32_t instance_id_;

  // Uniquely identifies an interaction in the current instance of
  // PageTimingMetadataRecorder. Intentionally 32-bit because it will be packed
  // with another 32-bit integer into a 64-bit integer.
  uint32_t interaction_count_ = 0;

  MonotonicTiming timing_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METADATA_RECORDER_H_
