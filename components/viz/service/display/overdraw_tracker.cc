// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overdraw_tracker.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

// Max number of average overdraw records. It is 30min when interval size
// is 1s and no tests should run longer than 30min.
constexpr size_t kMaxRecords = 1800u;

void LogAverageOverdrawCountUMA(float overdraw) {
  constexpr char kAverageOverdrawHistogramName[] =
      "Compositing.Display.Draw.AverageOverdraw2";
  // For optimal histogram bucketing, convert floating-point values into
  // integers while preserving the desired level of decimal precision.
  constexpr int kConversionFactor = 100'000;

  // The expected overdraw ranges is [1, 12].
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      kAverageOverdrawHistogramName,
      base::ClampRound<int>(overdraw * kConversionFactor),
      /*minimum=*/1 * kConversionFactor,
      /*maximum=*/(12 * kConversionFactor) + 1, /*bucket_count=*/50);
}

}  // namespace

// static
void OverdrawTracker::EstimateAndRecordOverdrawAsUMAMetric(
    const AggregatedFrame* frame) {
  const float overdraw = EstimateOverdraw(frame);
  LogAverageOverdrawCountUMA(overdraw);
}

// static
float OverdrawTracker::EstimateOverdraw(const AggregatedFrame* frame) {
  if (frame->render_pass_list.empty()) {
    return 0;
  }

  auto* root_render_pass = frame->render_pass_list.back().get();
  DCHECK(root_render_pass);

  const gfx::Rect& display_rect = root_render_pass->output_rect;

  base::CheckedNumeric<uint64_t> overdraw = 0;
  for (const auto& pass : frame->render_pass_list) {
    for (auto quad = pass->quad_list.begin(); quad != pass->quad_list.end();
         ++quad) {
      auto* sqs = quad->shared_quad_state;
      auto quad_to_root_transform = sqs->quad_to_target_transform;

      if (!quad_to_root_transform.NonDegeneratePreserves2dAxisAlignment()) {
        continue;
      }

      auto quad_rect = gfx::ToEnclosingRect(
          quad_to_root_transform.MapRect(gfx::RectF(quad->visible_rect)));
      if (sqs->clip_rect) {
        quad_rect.Intersect(sqs->clip_rect.value());
      }

      overdraw += quad_rect.size().GetCheckedArea();
    }
  }

  return static_cast<float>(overdraw.ValueOrDefault(0)) /
         display_rect.size().Area64();
}

OverdrawTracker::OverdrawTracker(const Settings& settings)
    : settings_(settings), start_time_(base::TimeTicks::Now()) {
  interval_overdraw_averages_.reserve(kMaxRecords);
}

OverdrawTracker::~OverdrawTracker() = default;

void OverdrawTracker::EstimateAndRecordOverdraw(const AggregatedFrame* frame,
                                                base::TimeTicks timestamp) {
  const float overdraw = EstimateOverdraw(frame);
  Record(overdraw, timestamp);
}

OverdrawTracker::OverdrawTimeSeries OverdrawTracker::TakeDataAsTimeSeries()
    const {
  OverdrawTimeSeries averages;

  // If the recorder has yet to track overdraw of a frame, return empty vector.
  if (interval_overdraw_averages_.empty()) {
    return averages;
  }

  averages.resize(interval_overdraw_averages_.size());

  std::transform(
      interval_overdraw_averages_.begin(), interval_overdraw_averages_.end(),
      averages.begin(),
      [](const DecomposedAverage& average) { return average.GetAverage(); });

  return averages;
}

void OverdrawTracker::Reset() {
  interval_overdraw_averages_.clear();
}

void OverdrawTracker::Record(float overdraw, base::TimeTicks timestamp) {
  DCHECK_LE(start_time_, timestamp);

  const size_t interval_index = GetIntervalIndex(timestamp);
  DCHECK_LT(interval_index, kMaxRecords);

  if (interval_index >= interval_overdraw_averages_.size()) {
    interval_overdraw_averages_.resize(interval_index + 1);
  }

  // Update the average overdraw of the current time interval.
  DecomposedAverage& interval_average =
      interval_overdraw_averages_.at(interval_index);
  interval_average.AddValue(overdraw);
}

size_t OverdrawTracker::GetIntervalIndex(base::TimeTicks timestamp) const {
  return (timestamp - start_time_).InSeconds() /
         settings_.interval_length_in_seconds;
}

}  // namespace viz
