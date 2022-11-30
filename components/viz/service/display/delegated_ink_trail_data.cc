// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_trail_data.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/prediction/kalman_predictor.h"
#include "ui/base/prediction/least_squares_predictor.h"
#include "ui/base/prediction/linear_predictor.h"
#include "ui/base/prediction/linear_resampling.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/delegated_ink_point.h"

namespace viz {

DelegatedInkTrailData::DelegatedInkTrailData() {
  std::string predictor = features::InkPredictor();
  std::string full_name = "Renderer.DelegatedInkTrail.PredictionExperiment";
  for (int i = 0; i < kNumberOfPredictionConfigs; ++i) {
    prediction_handlers_[i].metrics_handler =
        std::make_unique<ui::PredictionMetricsHandler>(
            base::StrCat({full_name, base::NumberToString(i)}));
    prediction_handlers_[i].predictor = CreatePredictor(predictor);
  }
  should_draw_predicted_ink_points_ = features::ShouldDrawPredictedInkPoints();
}

std::unique_ptr<ui::InputPredictor> DelegatedInkTrailData::CreatePredictor(
    std::string predictor) {
  if (predictor == features::kPredictorLinearResampling) {
    return std::make_unique<ui::LinearResampling>();
  } else if (predictor == features::kPredictorLinear1) {
    return std::make_unique<ui::LinearPredictor>(
        ui::LinearPredictor::EquationOrder::kFirstOrder);
  } else if (predictor == features::kPredictorLinear2) {
    return std::make_unique<ui::LinearPredictor>(
        ui::LinearPredictor::EquationOrder::kSecondOrder);
  } else if (predictor == features::kPredictorLsq) {
    return std::make_unique<ui::LeastSquaresPredictor>();
  }

  // if `kPredictorKalman` or default, create Kalman predictor
  unsigned int predictor_options =
      ui::KalmanPredictor::PredictionOptions::kHeuristicsEnabled |
      ui::KalmanPredictor::PredictionOptions::kDirectionCutOffEnabled;
  return std::make_unique<ui::KalmanPredictor>(predictor_options);
}

DelegatedInkTrailData::~DelegatedInkTrailData() = default;
DelegatedInkTrailData::PredictionHandler::PredictionHandler() = default;
DelegatedInkTrailData::PredictionHandler::~PredictionHandler() = default;

void DelegatedInkTrailData::AddPoint(const gfx::DelegatedInkPoint& point) {
  if (static_cast<int>(points_.size()) == 0)
    pointer_id_ = point.pointer_id();
  else
    DCHECK_EQ(pointer_id_, point.pointer_id());

  for (auto& it : prediction_handlers_)
    it.predictor->Update(
        ui::InputPredictor::InputData(point.point(), point.timestamp()));

  // Fail-safe to prevent storing excessive points if they are being sent but
  // never filtered and used, like if the renderer has stalled during a long
  // running script.
  if (points_.size() == gfx::kMaximumNumberOfDelegatedInkPoints)
    points_.erase(points_.begin());

  points_.insert({point.timestamp(), point.point()});
}

void DelegatedInkTrailData::PredictPoints(
    std::vector<gfx::DelegatedInkPoint>* ink_points_to_draw,
    gfx::DelegatedInkMetadata* metadata) {
  TRACE_EVENT0("delegated_ink_trails", "DelegatedInkTrailData::PredictPoints");
  // Base name used for the histograms that measure the latency improvement from
  // the prediction done for different experiments.
  static const char* histogram_base_name =
      "Renderer.DelegatedInkTrail.LatencyImprovementWithPrediction.Experiment";

  for (int experiment = 0; experiment < kNumberOfPredictionConfigs;
       ++experiment) {
    // Used to track the max amount of time predicted for each experiment. Since
    // prediction is disabled by default, we can't just check the last point of
    // |ink_points_to_draw| because the predicted points are only added there
    // when prediction is enabled.
    base::TimeDelta latency_improvement_with_prediction;

    PredictionHandler& handler = prediction_handlers_[experiment];

    if (handler.predictor->HasPrediction()) {
      for (int i = 0; i < kPredictionConfigs[experiment].points_to_predict;
           ++i) {
        base::TimeTicks timestamp =
            ink_points_to_draw->back().timestamp() +
            base::Milliseconds(kPredictionConfigs[experiment]
                                   .milliseconds_into_future_per_point);
        std::unique_ptr<ui::InputPredictor::InputData> predicted_point =
            handler.predictor->GeneratePrediction(timestamp);
        if (predicted_point) {
          handler.metrics_handler->AddPredictedEvent(
              predicted_point->pos, predicted_point->time_stamp,
              metadata->frame_time());
          latency_improvement_with_prediction =
              predicted_point->time_stamp - metadata->timestamp();
          if (should_draw_predicted_ink_points_.has_value() &&
              experiment == should_draw_predicted_ink_points_.value()) {
            ink_points_to_draw->push_back(gfx::DelegatedInkPoint(
                predicted_point->pos, predicted_point->time_stamp,
                pointer_id_));
          }
        } else {
          // HasPrediction() can return true while GeneratePrediction() fails to
          // produce a prediction if the predicted point would go in to the
          // opposite direction of most recently stored points. If this happens,
          // don't continue trying to generate more predicted points.
          break;
        }
      }
    }
    handler.metrics_handler->EvaluatePrediction();
    base::UmaHistogramTimes(
        base::StrCat({histogram_base_name, base::NumberToString(experiment)}),
        latency_improvement_with_prediction);
  }
}

void DelegatedInkTrailData::Reset() {
  for (auto& handler : prediction_handlers_) {
    handler.predictor->Reset();
    handler.metrics_handler->Reset();
  }
}

bool DelegatedInkTrailData::ContainsMatchingPoint(
    gfx::DelegatedInkMetadata* metadata) const {
  auto point = points_.find(metadata->timestamp());
  if (point == points_.end())
    return false;

  return gfx::DelegatedInkPoint(point->second, point->first)
      .MatchesDelegatedInkMetadata(metadata);
}

void DelegatedInkTrailData::ErasePointsOlderThanMetadata(
    gfx::DelegatedInkMetadata* metadata) {
  // Any points with a timestamp earlier than the metadata have already been
  // drawn by the app. Since the metadata timestamp will only increase, we can
  // safely erase every point earlier than it and be left only with the points
  // that can be drawn.
  while (points_.size() > 0 && points_.begin()->first < metadata->timestamp() &&
         points_.begin()->second != metadata->point())
    points_.erase(points_.begin());
}

void DelegatedInkTrailData::UpdateMetrics(gfx::DelegatedInkMetadata* metadata) {
  for (auto& handler : prediction_handlers_) {
    for (auto it : points_)
      handler.metrics_handler->AddRealEvent(it.second, it.first,
                                            metadata->frame_time());
  }
}

}  // namespace viz
