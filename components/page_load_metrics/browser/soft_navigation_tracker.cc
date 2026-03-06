// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/soft_navigation_tracker.h"

#include <utility>

#include "base/check_op.h"
#include "content/public/browser/global_routing_id.h"

namespace page_load_metrics {
//
// SoftNavigationTracker
//
SoftNavigationTracker::SoftNavigationTracker()
    : current_soft_navigation_(mojom::SoftNavigationMetrics::New()) {}

SoftNavigationTracker::~SoftNavigationTracker() = default;

bool SoftNavigationTracker::UpdateAndValidateMetrics(
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigation_metrics) {
  // We require each successful UpdateAndValidateMetrics call to be followed by
  // a loop that calls AdvanceToNextSoftNavigation() until all soft navigations
  // are processed; therefore soft_navigations_to_process_ should be empty.
  CHECK(soft_navigations_to_process_.empty());
  for (auto& soft_navigation : soft_navigation_metrics) {
    if (!ValidateIncoming(soft_navigation)) {
      soft_navigations_to_process_.clear();
      return false;
    }
    soft_navigations_to_process_.emplace_back(std::move(soft_navigation));
    ++soft_navigation_count_;
  }
  return true;
}

bool SoftNavigationTracker::ValidateIncoming(
    const mojom::SoftNavigationMetricsPtr& soft_navigation) {
  // TODO(johannes): Report invalid soft navigation metrics. crbug.com/490096674
  if (soft_navigation->soft_navigation_offset == 0 ||
      soft_navigation->soft_navigation_slicing_time.is_null() ||
      soft_navigation->same_document_metrics_token.is_empty()) {
    return false;
  }
  // Soft navigation metrics are expected to be sent in order, and we expect
  // to receive a contiguous range of soft navigation offsets.
  uint64_t expected_offset =
      soft_navigations_to_process_.empty()
          ? current_soft_navigation_->soft_navigation_offset + 1
          : soft_navigations_to_process_.back()->soft_navigation_offset + 1;
  if (soft_navigation->soft_navigation_offset != expected_offset) {
    return false;
  }
  // We expect the slicing time to be strictly monotonically increasing.
  base::TimeTicks previous_slicing_time =
      soft_navigations_to_process_.empty()
          ? current_soft_navigation_->soft_navigation_slicing_time
          : soft_navigations_to_process_.back()->soft_navigation_slicing_time;
  if (!previous_slicing_time.is_null() &&
      soft_navigation->soft_navigation_slicing_time <= previous_slicing_time) {
    return false;
  }
  // We expect the token to be different from the previous soft navigation.
  return soft_navigation->same_document_metrics_token !=
         current_soft_navigation_->same_document_metrics_token;
}

bool SoftNavigationTracker::HasNextSoftNavigation() const {
  return !soft_navigations_to_process_.empty();
}

void SoftNavigationTracker::AdvanceToNextSoftNavigation() {
  CHECK(!soft_navigations_to_process_.empty());
  current_soft_navigation_ = std::move(soft_navigations_to_process_.front());
  soft_navigations_to_process_.pop_front();
}

base::TimeTicks SoftNavigationTracker::SoftNavigationSlicingTime() const {
  return soft_navigations_to_process_.empty()
             ? base::TimeTicks()
             : soft_navigations_to_process_.front()
                   ->soft_navigation_slicing_time;
}

namespace {
// This template function is used to process the metrics that are associated
// with a soft navigation. It is used to process event timings, layout shifts,
// and LCP candidates.
// The template parameter T is a struct that contains the following:
// - MeasurementPtr: The pointer type of the metrics to process.
// - LimitType: The type of the limit that is used to determine whether a
// metric should be processed.
// - Calculator: The type of the calculator that is used to process the
// metrics.
// - ShouldProcess: A static function that returns true if the metric should be
// processed.
// - AddNewMeasurements: A static function that adds the metrics to the
// calculator.
template <typename T>
size_t ProcessTmpl(typename T::LimitType limit,
                 base::span<const typename T::MeasurementPtr>* measurements,
                 typename T::Calculator* calculator) {
  if (measurements->empty()) {
    return 0;
  }
  size_t ii = 0;
  while (ii < measurements->size() &&
         T::ShouldProcess(limit, (*measurements)[ii])) {
    ++ii;
  }
  auto measurements_to_process = measurements->first(ii);
  *measurements = measurements->subspan(ii);
  if (!measurements_to_process.empty()) {
    T::AddNewMeasurements(calculator, measurements_to_process);
  }
  return measurements_to_process.size();
}

// This struct is used to process event timings, for InteractionToNextPaint.
struct InteractionToNextPaintAdapter {
  using MeasurementPtr = mojom::EventTimingPtr;
  using LimitType = base::TimeTicks;
  using Calculator = InteractionToNextPaintCalculator;
  static bool ShouldProcess(base::TimeTicks limit,
                            const mojom::EventTimingPtr& measurement) {
    return limit.is_null() || measurement->start_time <= limit;
  }
  static void AddNewMeasurements(
      InteractionToNextPaintCalculator* calculator,
      base::span<const mojom::EventTimingPtr> measurements) {
    calculator->AddNewEventTimings(content::GlobalRenderFrameHostToken(),
                                   measurements);
  }
};

// This struct is used to process layout shifts, for CumulativeLayoutShift.
struct LayoutShiftAdapter {
  using MeasurementPtr = mojom::LayoutShiftPtr;
  using LimitType = base::TimeTicks;
  using Calculator = LayoutShiftNormalization;
  static bool ShouldProcess(base::TimeTicks limit,
                            const mojom::LayoutShiftPtr& measurement) {
    return limit.is_null() || measurement->layout_shift_time <= limit;
  }
  static void AddNewMeasurements(
      LayoutShiftNormalization* calculator,
      base::span<const MeasurementPtr> measurements) {
    calculator->AddNewLayoutShifts(measurements, base::TimeTicks::Now());
  }
};

// This struct is used to process LCP candidates, for LargestContentfulPaint.
struct LargestContentfulPaintAdapter {
  using MeasurementPtr = mojom::LargestContentfulPaintTimingPtr;
  using LimitType = uint64_t;
  using Calculator = ContentfulPaint;
  static bool ShouldProcess(
      uint64_t limit,
      const mojom::LargestContentfulPaintTimingPtr& measurement) {
    return measurement->soft_navigation_offset <= limit;
  }
  static void AddNewMeasurements(
      ContentfulPaint* calculator,
      base::span<const mojom::LargestContentfulPaintTimingPtr> measurements) {
    for (const auto& measurement : measurements) {
      if (measurement->largest_text_paint.has_value()) {
        // Image load start/end are not applicable to text LCP elements.
        calculator->Text().Reset(
            measurement->largest_text_paint,
            measurement->largest_text_paint_size,
            static_cast<blink::LargestContentfulPaintType>(measurement->type),
            /*image_bpp=*/0.0,
            /*image_request_priority=*/std::nullopt,
            /*image_discovery_time=*/std::nullopt,
            /*image_load_start=*/std::nullopt,
            /*image_load_end=*/std::nullopt);
      }
      if (measurement->largest_image_paint.has_value()) {
        std::optional<net::RequestPriority> request_priority;
        if (measurement->image_request_priority_valid) {
          request_priority = measurement->image_request_priority_value;
        }
        calculator->Image().Reset(
            measurement->largest_image_paint,
            measurement->largest_image_paint_size,
            static_cast<blink::LargestContentfulPaintType>(measurement->type),
            measurement->image_bpp, request_priority,
            measurement->resource_load_timings->discovery_time,
            measurement->resource_load_timings->load_start,
            measurement->resource_load_timings->load_end);
      }
    }
  }
};
}  // namespace

size_t SoftNavigationTracker::Process(
    base::span<const mojom::EventTimingPtr>* event_timings,
    InteractionToNextPaintCalculator* calculator) const {
  return ProcessTmpl<InteractionToNextPaintAdapter>(SoftNavigationSlicingTime(),
                                             event_timings, calculator);
}

size_t SoftNavigationTracker::Process(
    base::span<const mojom::LayoutShiftPtr>* layout_shifts,
    LayoutShiftNormalization* layout_shift_normalization) const {
  return ProcessTmpl<LayoutShiftAdapter>(SoftNavigationSlicingTime(), layout_shifts,
                                  layout_shift_normalization);
}

size_t SoftNavigationTracker::Process(
    base::span<const mojom::LargestContentfulPaintTimingPtr>* soft_lcps,
    ContentfulPaint* soft_lcp_candidate) const {
  return ProcessTmpl<LargestContentfulPaintAdapter>(
      current_soft_navigation().soft_navigation_offset, soft_lcps,
      soft_lcp_candidate);
}

}  // namespace page_load_metrics
