// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_TRAIL_DATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_TRAIL_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/delegated_ink_prediction_configuration.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/base/prediction/prediction_metrics_handler.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class DelegatedInkMetadata;
class DelegatedInkPoint;
}  // namespace gfx

namespace ui {
class InputPredictor;
}  // namespace ui

namespace viz {

class VIZ_SERVICE_EXPORT DelegatedInkTrailData {
 public:
  DelegatedInkTrailData();
  ~DelegatedInkTrailData();
  std::unique_ptr<ui::InputPredictor> CreatePredictor(std::string predictor);

  void AddPoint(const gfx::DelegatedInkPoint& point);
  void PredictPoints(std::vector<gfx::DelegatedInkPoint>* ink_points_to_draw,
                     gfx::DelegatedInkMetadata* metadata);
  void Reset();
  bool ContainsMatchingPoint(gfx::DelegatedInkMetadata* metadata) const;
  void ErasePointsOlderThanMetadata(gfx::DelegatedInkMetadata* metadata);
  void UpdateMetrics(gfx::DelegatedInkMetadata* metadata);

  const std::map<base::TimeTicks, gfx::PointF>& GetPoints() const {
    return points_;
  }

 private:
  // Helper struct for holding the predictor and matching metrics handler. This
  // only needs to exist during ongoing prediction experimentation. Once the
  // experimentation is complete, only a single predictor and single metrics
  // handler will be needed.
  struct PredictionHandler {
   public:
    PredictionHandler();
    ~PredictionHandler();
    // Kalman predictors that are used for predicting points.
    std::unique_ptr<ui::InputPredictor> predictor;

    // Handlers for calculating useful metrics for evaluating predicted points
    // and populating the histograms with those metrics.
    std::unique_ptr<ui::PredictionMetricsHandler> metrics_handler;
  };

  // Array of the predictors and matching metrics handlers for doing prediction
  // experimentation.
  PredictionHandler prediction_handlers_[kNumberOfPredictionConfigs];

  // The points that arrived from the browser process and will be used to draw
  // the delegated ink trail.
  std::map<base::TimeTicks, gfx::PointF> points_;

  // The pointer id associated with these points.
  int32_t pointer_id_;

  // Id for prediction configuration to draw. No value if disabled.
  absl::optional<int> should_draw_predicted_ink_points_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_TRAIL_DATA_H_
