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
#include "components/viz/service/viz_service_export.h"
#include "ui/base/prediction/prediction_metrics_handler.h"
#include "ui/gfx/delegated_ink_point.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class DelegatedInkMetadata;
}  // namespace gfx

namespace ui {
class LinearResampling;
}  // namespace ui

namespace viz {

class VIZ_SERVICE_EXPORT DelegatedInkTrailData {
 public:
  DelegatedInkTrailData();
  ~DelegatedInkTrailData();

  void AddPoint(const gfx::DelegatedInkPoint& point);
  void PredictPoints(std::vector<gfx::DelegatedInkPoint>* ink_points_to_draw,
                     gfx::DelegatedInkMetadata* metadata);
  void Reset();
  bool ContainsMatchingPoint(gfx::DelegatedInkMetadata* metadata) const;
  gfx::DelegatedInkPoint GetMatchingPoint(
      gfx::DelegatedInkMetadata* metadata) const;
  void ErasePointsOlderThanMetadata(gfx::DelegatedInkMetadata* metadata);
  void UpdateMetrics(gfx::DelegatedInkMetadata* metadata);

  std::map<base::TimeTicks, gfx::DelegatedInkPoint>& GetPoints() {
    return points_;
  }

  const std::map<base::TimeTicks, gfx::DelegatedInkPoint>& GetPoints() const {
    return points_;
  }

 private:
  // Handler for calculating useful metrics for evaluating predicted points
  // and populating the histograms with those metrics.
  ui::PredictionMetricsHandler metrics_handler_;

  // Predictor for improving the perceived benefits of using Delegated Ink.
  std::unique_ptr<ui::LinearResampling> predictor_;

  // The points that arrived from the browser process and will be used to draw
  // the delegated ink trail.
  std::map<base::TimeTicks, gfx::DelegatedInkPoint> points_;

  // The pointer id associated with these points.
  int32_t pointer_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_TRAIL_DATA_H_
