// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_BASE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_BASE_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/delegated_ink_point.mojom.h"
#include "ui/base/prediction/input_predictor.h"
#include "ui/base/prediction/prediction_metrics_handler.h"

namespace viz {
class DelegatedInkMetadata;

// The maximum number of delegated ink points that will be stored at a time.
// When this is hit, the oldest one will be removed each time a new one is
// added.
constexpr int kMaximumDelegatedInkPointsStored = 10;

// The number of points to predict into the future, when prediction is
// available.
constexpr int kNumberOfPointsToPredict = 1;

// The time that each predicted point should be ahead of the previous point,
// in milliseconds.
constexpr int kNumberOfMillisecondsIntoFutureToPredictPerPoint = 12;

// This is the base class used for rendering delegated ink trails on the end of
// strokes to reduce user perceived latency. On initialization, it binds the
// mojo interface required for receiving delegated ink points that are made and
// sent from the browser process.
//
// For more information on the feature, please see the explainer:
// https://github.com/WICG/ink-enhancement/blob/master/README.md
class VIZ_SERVICE_EXPORT DelegatedInkPointRendererBase
    : public mojom::DelegatedInkPointRenderer {
 public:
  DelegatedInkPointRendererBase();
  ~DelegatedInkPointRendererBase() override;
  DelegatedInkPointRendererBase(const DelegatedInkPointRendererBase&) = delete;
  DelegatedInkPointRendererBase& operator=(
      const DelegatedInkPointRendererBase&) = delete;

  void InitMessagePipeline(
      mojo::PendingReceiver<mojom::DelegatedInkPointRenderer> receiver);

  void StoreDelegatedInkPoint(const DelegatedInkPoint& point) override;
  void SetDelegatedInkMetadata(std::unique_ptr<DelegatedInkMetadata> metadata);

  virtual void FinalizePathForDraw() = 0;
  virtual gfx::Rect GetDamageRect() = 0;

 protected:
  // |points_| is not emptied each time after the points are drawn, because one
  // point in |points_| could potentially be drawn in more than one delegated
  // ink trail. However, if a point has a timestamp that is earlier than the
  // timestamp on the metadata, then the point has already been drawn, and
  // therefore should be removed from |points_| before drawing.
  std::vector<DelegatedInkPoint> FilterPoints();

  void PredictPoints(std::vector<DelegatedInkPoint>* ink_points_to_draw);
  void ResetPrediction();

  std::unique_ptr<DelegatedInkMetadata> metadata_;

 private:
  friend class SkiaDelegatedInkRendererTest;

  const std::map<base::TimeTicks, gfx::PointF>& GetPointsMapForTest() const {
    return points_;
  }

  const DelegatedInkMetadata* GetMetadataForTest() const {
    return metadata_.get();
  }

  virtual int GetPathPointCountForTest() const = 0;

  // The points that arrived from the browser process and may be drawn as part
  // of the ink trail.
  std::map<base::TimeTicks, gfx::PointF> points_;

  // Kalman predictor that is used for generating predicted points.
  std::unique_ptr<ui::InputPredictor> predictor_;

  // Handler for calculating useful metrics for evaluating predicted points
  // and populating the histograms with those metrics.
  ui::PredictionMetricsHandler metrics_handler_;

  mojo::Receiver<mojom::DelegatedInkPointRenderer> receiver_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_BASE_H_
