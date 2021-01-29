// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_TRAIL_DATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_TRAIL_DATA_H_

#include <map>
#include <memory>

#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/base/prediction/input_predictor.h"
#include "ui/base/prediction/prediction_metrics_handler.h"
#include "ui/gfx/geometry/point_f.h"

namespace viz {
class DelegatedInkMetadata;
class DelegatedInkPoint;

// The maximum number of delegated ink points that will be stored at a time.
// When this is hit, the oldest one will be removed each time a new one is
// added.
constexpr int kMaximumDelegatedInkPointsStored = 10;

class VIZ_SERVICE_EXPORT DelegatedInkTrailData {
 public:
  DelegatedInkTrailData();
  ~DelegatedInkTrailData();

  void AddPoint(const DelegatedInkPoint& point);
  base::Optional<DelegatedInkPoint> GetPredictedPoint(
      base::TimeTicks timestamp,
      base::TimeTicks frame_time);
  void Reset();
  bool ContainsMatchingPoint(DelegatedInkMetadata* metadata) const;
  void ErasePointsOlderThanMetadata(DelegatedInkMetadata* metadata);
  void UpdateMetrics(DelegatedInkMetadata* metadata);

  const std::map<base::TimeTicks, gfx::PointF>& GetPoints() const {
    return points_;
  }
  bool HasPrediction() const { return predictor_->HasPrediction(); }
  void EvaluatePrediction() { metrics_handler_.EvaluatePrediction(); }

 private:
  // The points that arrived from the browser process and will be used to draw
  // the delegated ink trail.
  std::map<base::TimeTicks, gfx::PointF> points_;

  // Kalman predictor that is used for generating predicted points.
  std::unique_ptr<ui::InputPredictor> predictor_;

  // Handler for calculating useful metrics for evaluating predicted points
  // and populating the histograms with those metrics.
  ui::PredictionMetricsHandler metrics_handler_;

  // The pointer id associated with these points.
  int32_t pointer_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_TRAIL_DATA_H_
