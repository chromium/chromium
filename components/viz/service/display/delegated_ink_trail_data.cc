// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_trail_data.h"

#include "components/viz/common/delegated_ink_metadata.h"
#include "components/viz/common/delegated_ink_point.h"
#include "ui/base/prediction/kalman_predictor.h"

namespace viz {

DelegatedInkTrailData::DelegatedInkTrailData()
    : metrics_handler_("Renderer.DelegatedInkTrail.Prediction") {
  unsigned int predictor_options =
      ui::KalmanPredictor::PredictionOptions::kHeuristicsEnabled |
      ui::KalmanPredictor::PredictionOptions::kDirectionCutOffEnabled;
  predictor_ = std::make_unique<ui::KalmanPredictor>(predictor_options);
}

DelegatedInkTrailData::~DelegatedInkTrailData() = default;

void DelegatedInkTrailData::AddPoint(const DelegatedInkPoint& point) {
  if (static_cast<int>(points_.size()) == 0)
    pointer_id_ = point.pointer_id();
  else
    DCHECK_EQ(pointer_id_, point.pointer_id());

  predictor_->Update(
      ui::InputPredictor::InputData(point.point(), point.timestamp()));

  // Fail-safe to prevent storing excessive points if they are being sent but
  // never filtered and used, like if the renderer has stalled during a long
  // running script.
  if (points_.size() == kMaximumDelegatedInkPointsStored)
    points_.erase(points_.begin());

  points_.insert({point.timestamp(), point.point()});
}

base::Optional<DelegatedInkPoint> DelegatedInkTrailData::GetPredictedPoint(
    base::TimeTicks timestamp,
    base::TimeTicks frame_time) {
  std::unique_ptr<ui::InputPredictor::InputData> predicted_point =
      predictor_->GeneratePrediction(timestamp);
  if (!predicted_point)
    return base::nullopt;

  metrics_handler_.AddPredictedEvent(predicted_point->pos,
                                     predicted_point->time_stamp, frame_time);
  return DelegatedInkPoint(predicted_point->pos, predicted_point->time_stamp,
                           pointer_id_);
}

void DelegatedInkTrailData::Reset() {
  predictor_->Reset();
  metrics_handler_.Reset();
}

bool DelegatedInkTrailData::ContainsMatchingPoint(
    DelegatedInkMetadata* metadata) const {
  auto point = points_.find(metadata->timestamp());
  if (point == points_.end())
    return false;

  return point->second == metadata->point();
}

void DelegatedInkTrailData::ErasePointsOlderThanMetadata(
    DelegatedInkMetadata* metadata) {
  // Any points with a timestamp earlier than the metadata have already been
  // drawn by the app. Since the metadata timestamp will only increase, we can
  // safely erase every point earlier than it and be left only with the points
  // that can be drawn.
  while (points_.size() > 0 && points_.begin()->first < metadata->timestamp() &&
         points_.begin()->second != metadata->point())
    points_.erase(points_.begin());
}

void DelegatedInkTrailData::UpdateMetrics(DelegatedInkMetadata* metadata) {
  for (auto it : points_)
    metrics_handler_.AddRealEvent(it.second, it.first, metadata->frame_time());
}

}  // namespace viz
