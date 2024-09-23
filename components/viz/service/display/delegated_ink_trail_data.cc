// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display/delegated_ink_trail_data.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "ui/base/prediction/linear_resampling.h"
#include "ui/gfx/delegated_ink_metadata.h"

namespace viz {

namespace {
// Values used to configure the points predictor.
static const int kPointsToPredict = 2;
static const int kMillisecondsIntoFuturePerPoint = 6;
}  // namespace

DelegatedInkTrailData::DelegatedInkTrailData()
    : metrics_handler_("Renderer.DelegatedInkTrail.Prediction"),
      predictor_(std::make_unique<ui::LinearResampling>()) {}

DelegatedInkTrailData::~DelegatedInkTrailData() = default;

void DelegatedInkTrailData::AddPoint(const gfx::DelegatedInkPoint& point) {
  if (points_.empty()) {
    pointer_id_ = point.pointer_id();
  }
  CHECK_EQ(pointer_id_, point.pointer_id());

  predictor_->Update(
      ui::InputPredictor::InputData(point.point(), point.timestamp()));

  // Fail-safe to prevent storing excessive points if they are being sent but
  // never filtered and used, like if the renderer has stalled during a long
  // running script.
  if (points_.size() == gfx::kMaximumNumberOfDelegatedInkPoints) {
    points_.erase(points_.begin());
  }

  points_.insert({point.timestamp(), point});
}

void DelegatedInkTrailData::PredictPoints(
    std::vector<gfx::DelegatedInkPoint>* ink_points_to_draw,
    gfx::DelegatedInkMetadata* metadata) {
  if (!features::ShouldDrawPredictedInkPoints()) {
    return;
  }

  TRACE_EVENT0("delegated_ink_trails", "DelegatedInkTrailData::PredictPoints");

  base::TimeDelta latency_improvement_with_prediction;
  if (predictor_->HasPrediction()) {
    for (int i = 0; i < kPointsToPredict; ++i) {
      base::TimeTicks timestamp =
          ink_points_to_draw->back().timestamp() +
          base::Milliseconds(kMillisecondsIntoFuturePerPoint);
      std::unique_ptr<ui::InputPredictor::InputData> predicted_point =
          predictor_->GeneratePrediction(timestamp, base::Seconds(0));
      metrics_handler_.AddPredictedEvent(predicted_point->pos,
                                         predicted_point->time_stamp,
                                         metadata->frame_time());
      latency_improvement_with_prediction =
          predicted_point->time_stamp - metadata->timestamp();
      ink_points_to_draw->emplace_back(
          predicted_point->pos, predicted_point->time_stamp, pointer_id_);
    }
  }
  metrics_handler_.EvaluatePrediction();
  base::UmaHistogramTimes(
      "Renderer.DelegatedInkTrail.LatencyImprovement.Skia.WithPrediction",
      latency_improvement_with_prediction);
}

void DelegatedInkTrailData::Reset() {
  predictor_->Reset();
  metrics_handler_.Reset();
}

bool DelegatedInkTrailData::ContainsMatchingPoint(
    gfx::DelegatedInkMetadata* metadata) const {
  const auto& point = points_.find(metadata->timestamp());
  return point != points_.end() &&
         point->second.MatchesDelegatedInkMetadata(metadata);
}

gfx::DelegatedInkPoint DelegatedInkTrailData::GetMatchingPoint(
    gfx::DelegatedInkMetadata* metadata) const {
  CHECK(ContainsMatchingPoint(metadata));
  return points_.find(metadata->timestamp())->second;
}

void DelegatedInkTrailData::ErasePointsOlderThanMetadata(
    gfx::DelegatedInkMetadata* metadata) {
  // Any points with a timestamp earlier than the metadata have already been
  // drawn by the app. Since the metadata timestamp will only increase, we can
  // safely erase every point earlier than it and be left only with the points
  // that can be drawn.
  while (!points_.empty() &&
         points_.begin()->second.timestamp() < metadata->timestamp()) {
    points_.erase(points_.begin());
  }
}

void DelegatedInkTrailData::UpdateMetrics(gfx::DelegatedInkMetadata* metadata) {
  for (const auto& [_, point] : points_) {
    metrics_handler_.AddRealEvent(point.point(), point.timestamp(),
                                  metadata->frame_time());
  }
}

}  // namespace viz
