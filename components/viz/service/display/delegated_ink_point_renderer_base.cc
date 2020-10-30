// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_renderer_base.h"

#include "base/trace_event/trace_event.h"
#include "components/viz/common/delegated_ink_metadata.h"
#include "ui/base/prediction/kalman_predictor.h"

namespace viz {

DelegatedInkPointRendererBase::DelegatedInkPointRendererBase()
    : metrics_handler_("Renderer.DelegatedInkTrail.Prediction") {
  unsigned int predictor_options =
      ui::KalmanPredictor::PredictionOptions::kHeuristicsEnabled |
      ui::KalmanPredictor::PredictionOptions::kDirectionCutOffEnabled;
  predictor_ = std::make_unique<ui::KalmanPredictor>(predictor_options);
}
DelegatedInkPointRendererBase::~DelegatedInkPointRendererBase() = default;

void DelegatedInkPointRendererBase::InitMessagePipeline(
    mojo::PendingReceiver<mojom::DelegatedInkPointRenderer> receiver) {
  // The remote end of this pipeline exists on a per-tab basis, so if tab A
  // is using the feature and then tab B starts trying to use it, a new
  // PendingReceiver will arrive here while |receiver_| is still bound to the
  // remote in tab A. In this case, just reset |receiver_| so that tab A's
  // remote is unbound and bind the new receiver to use the feature in tab B.
  if (receiver_.is_bound()) {
    receiver_.reset();
    metadata_.reset();
    points_.clear();
  }
  receiver_.Bind(std::move(receiver));
}

void DelegatedInkPointRendererBase::SetDelegatedInkMetadata(
    std::unique_ptr<DelegatedInkMetadata> metadata) {
  // Frame time is set later than everything else due to what is available
  // at time of creation, so confirm that it was actually set.
  DCHECK_NE(metadata->frame_time(), base::TimeTicks());
  metadata_ = std::move(metadata);
}

std::vector<DelegatedInkPoint> DelegatedInkPointRendererBase::FilterPoints() {
  if (points_.size() == 0)
    return {};

  DCHECK(metadata_);

  // TODO(1052145): Add additional filtering to prevent points in |points_| from
  // having a timestamp that is far ahead of |metadata_|'s timestamp. This could
  // occur if the renderer stalls before sending a metadata while the browser
  // continues to pump points through to viz. Then when the renderer starts back
  // up again, the metadata it sends may be significantly older than the points
  // stored here, resulting in a long possibly incorrect trail if the max
  // number of points to store was reached.

  // First remove all points from |points_| with timestamps earlier than
  // |metadata_|, as they have already been rendered by the app and are no
  // longer useful for a trail.
  // After that, there are three possible state of |points_|:
  //   1. The earliest DelegatedInkPoint in |points_| matches |metadata_|'s
  //      timestamp. All the points in |points_| can be used to draw a trail.
  //   2. |points_| is empty. No DelegatedInkPoints arrived from the browser
  //      process with a timestamp equal to or later than |metadata_|'s, so we
  //      don't have any points to make a trail from.
  //   3. There are DelegatedInkPoints in |points_|, but the earliest one is
  //      later than |metadata_|. This can happen most often when the API is
  //      first used, as the browser process did not know to send the point
  //      to viz before it was used to make the metadata in the renderer. So
  //      although it didn't send the DelegatedInkPoint matching |metadata_|, it
  //      still may have sent future points before the metadata propagated all
  //      the way here. In this case, we choose not to use the points in
  //      |points_| to draw, as we have no way of confirming that there
  //      shouldn't be any extra points between |metadata_| and the beginning
  //      of |points_|. So instead, just leave everything after |metadata_| in
  //      |points_| so that they may be used in future trails and don't draw
  //      any trail for the current |metadata_|.
  // So if |points_| contains a timestamp that matches |metadata_|'s timestamp,
  // add it and every point after it to |points_to_draw| and return it for
  // drawing. If it doesn't, just return an empty vector and leave any point
  // with a timestamp later than |metadata_|'s in |points_|.
  std::vector<DelegatedInkPoint> points_to_draw;

  auto it = points_.begin();
  while (points_.size() > 0 && it != points_.end()) {
    if (it->first < metadata_->timestamp()) {
      // Sanity check to confirm that we always find the points that are before
      // |metadata_|'s timestamp at the beginning of |points_| since it should
      // be sorted.
      DCHECK(it == points_.begin());
      it = points_.erase(it);
    } else {
      if (it->first == metadata_->timestamp() || points_to_draw.size() > 0) {
        points_to_draw.emplace_back(it->second, it->first);
        metrics_handler_.AddRealEvent(it->second, it->first,
                                      metadata_->frame_time());
        it++;
      } else {
        // If we find a point that is later than |metadata_|'s timestamp before
        // finding one that matches |metadata_|'s timestamp, that means that
        // it doesn't exist in |points_|, so return an empty vector as there are
        // no valid points to draw.
        break;
      }
    }
  }

  return points_to_draw;
}

void DelegatedInkPointRendererBase::PredictPoints(
    std::vector<DelegatedInkPoint>* ink_points_to_draw) {
  DCHECK(metadata_);
  int points_predicted = 0;

  // |ink_points_to_draw| needs to have at least one point in it already as a
  // reference to know what timestamp to start predicting points at. This single
  // point may just match |metadata_|.
  if (predictor_->HasPrediction() && ink_points_to_draw->size() > 0) {
    for (int i = 0; i < kNumberOfPointsToPredict; ++i) {
      base::TimeTicks timestamp =
          ink_points_to_draw->back().timestamp() +
          base::TimeDelta::FromMilliseconds(
              kNumberOfMillisecondsIntoFutureToPredictPerPoint);
      std::unique_ptr<ui::InputPredictor::InputData> predicted_point =
          predictor_->GeneratePrediction(timestamp);
      if (predicted_point) {
        ink_points_to_draw->emplace_back(predicted_point->pos,
                                         predicted_point->time_stamp);
        metrics_handler_.AddPredictedEvent(predicted_point->pos,
                                           predicted_point->time_stamp,
                                           metadata_->frame_time());
        points_predicted++;
      } else {
        // HasPrediction() can return true while GeneratePrediction() fails to
        // produce a prediction if the predicted point would go in to the
        // opposite direction of most recently stored points. If this happens,
        // don't continue trying to generate more predicted points.
        break;
      }
    }
  }

  TRACE_EVENT_INSTANT1("viz", "DelegatedInkPointRendererBase::PredictPoints",
                       TRACE_EVENT_SCOPE_THREAD, "predicted points",
                       points_predicted);

  metrics_handler_.EvaluatePrediction();
}

void DelegatedInkPointRendererBase::ResetPrediction() {
  predictor_->Reset();
  metrics_handler_.Reset();
}

void DelegatedInkPointRendererBase::StoreDelegatedInkPoint(
    const DelegatedInkPoint& point) {
  TRACE_EVENT_INSTANT1("viz",
                       "DelegatedInkPointRendererImpl::StoreDelegatedInkPoint",
                       TRACE_EVENT_SCOPE_THREAD, "point", point.ToString());

  predictor_->Update(
      ui::InputPredictor::InputData(point.point(), point.timestamp()));

  // Fail-safe to prevent storing excessive points if they are being sent but
  // never filtered and used, like if the renderer has stalled during a long
  // running script.
  if (points_.size() == kMaximumDelegatedInkPointsStored)
    points_.erase(points_.begin());

  points_.insert({point.timestamp(), point.point()});
}

}  // namespace viz
