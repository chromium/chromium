// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_renderer_base.h"

#include "base/trace_event/trace_event.h"
#include "components/viz/common/delegated_ink_metadata.h"
#include "components/viz/service/display/delegated_ink_trail_data.h"

namespace viz {

DelegatedInkPointRendererBase::DelegatedInkPointRendererBase() = default;
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
    pointer_ids_.clear();
  }
  receiver_.Bind(std::move(receiver));
}

void DelegatedInkPointRendererBase::SetDelegatedInkMetadata(
    std::unique_ptr<DelegatedInkMetadata> metadata) {
  // Frame time is set later than everything else due to what is available
  // at time of creation, so confirm that it was actually set.
  DCHECK_NE(metadata->frame_time(), base::TimeTicks());
  metadata_ = std::move(metadata);

  // If we already have a cached pointer ID, check if the same pointer ID
  // matches the new metadata.
  if (pointer_id_.has_value() &&
      pointer_ids_[pointer_id_.value()].ContainsMatchingPoint(
          metadata_.get())) {
    return;
  }

  // If not, find the pointer ID that does match it, if any, and cache it.
  for (auto& it : pointer_ids_) {
    if (it.second.ContainsMatchingPoint(metadata_.get())) {
      pointer_id_ = it.first;
      return;
    }
  }

  // If we aren't able to find any matching point, set the pointer ID to null
  // so that FilterPoints and PredictPoints can early out.
  pointer_id_ = base::nullopt;
}

std::vector<DelegatedInkPoint> DelegatedInkPointRendererBase::FilterPoints() {
  if (pointer_ids_.size() == 0)
    return {};

  DCHECK(metadata_);

  // Any stored point with a timestamp earlier than the metadata's has already
  // been drawn as part of the ink stroke and therefore should not be part of
  // the delegated ink trail. Do this before checking if |pointer_id_| is valid
  // because it helps manage the number of DelegatedInkPoints that are being
  // stored and isn't dependent on |pointer_id_| at all.
  for (auto& it : pointer_ids_)
    it.second.ErasePointsOlderThanMetadata(metadata_.get());

  // TODO(1052145): Add additional filtering to prevent points in |points_| from
  // having a timestamp that is far ahead of |metadata_|'s timestamp. This could
  // occur if the renderer stalls before sending a metadata while the browser
  // continues to pump points through to viz. Then when the renderer starts back
  // up again, the metadata it sends may be significantly older than the points
  // stored here, resulting in a long possibly incorrect trail if the max
  // number of points to store was reached.

  // If no point with any pointer id exactly matches the metadata, then we can't
  // confirm which set of points to use for the delegated ink trail, so just
  // return an empty vector so that nothing will be drawn. This happens most
  // often at the beginning of delegated ink trail use. The metadata is created
  // using a PointerEvent earlier than any DelegatedInkPoint is created,
  // resulting in the metadata having an earlier timestamp and a point that
  // doesn't match anything that is sent here from viz. Even if only a single
  // pointer ID is in use, we can't know with any certainty what happened
  // between the metadata point and the earliest DelegatedInkPoint we have, so
  // we choose to just not draw anything.
  if (!pointer_id_.has_value())
    return {};

  DelegatedInkTrailData& trail_data = pointer_ids_[pointer_id_.value()];

  // Make sure the metrics handler is provided the new real events to accurately
  // measure the prediction later.
  trail_data.UpdateMetrics(metadata_.get());

  // Any remaining points must be the points that should be part of the
  // delegated ink trail
  std::vector<DelegatedInkPoint> points_to_draw;
  for (auto it : trail_data.GetPoints())
    points_to_draw.emplace_back(it.second, it.first, pointer_id_.value());

  DCHECK(points_to_draw.front().point() == metadata_->point() &&
         points_to_draw.front().timestamp() == metadata_->timestamp());

  return points_to_draw;
}

void DelegatedInkPointRendererBase::PredictPoints(
    std::vector<DelegatedInkPoint>* ink_points_to_draw) {
  DCHECK(metadata_);

  if (!pointer_id_.has_value())
    return;

  DelegatedInkTrailData& trail_data = pointer_ids_[pointer_id_.value()];
  int points_predicted = 0;

  // |ink_points_to_draw| needs to have at least one point in it already as a
  // reference to know what timestamp to start predicting points at. This single
  // point may just match |metadata_|.
  if (trail_data.HasPrediction() && ink_points_to_draw->size() > 0) {
    for (int i = 0; i < kNumberOfPointsToPredict; ++i) {
      base::TimeTicks timestamp =
          ink_points_to_draw->back().timestamp() +
          base::TimeDelta::FromMilliseconds(
              kNumberOfMillisecondsIntoFutureToPredictPerPoint);
      base::Optional<DelegatedInkPoint> predicted_point =
          trail_data.GetPredictedPoint(timestamp, metadata_->frame_time());
      if (predicted_point.has_value()) {
        ink_points_to_draw->push_back(predicted_point.value());
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

  if (points_predicted > 0)
    trail_data.EvaluatePrediction();
}

void DelegatedInkPointRendererBase::ResetPrediction() {
  for (auto& it : pointer_ids_)
    it.second.Reset();
  TRACE_EVENT_INSTANT0("viz", "Delegated ink prediction reset.",
                       TRACE_EVENT_SCOPE_THREAD);
}

void DelegatedInkPointRendererBase::StoreDelegatedInkPoint(
    const DelegatedInkPoint& point) {
  TRACE_EVENT_INSTANT1("viz",
                       "DelegatedInkPointRendererImpl::StoreDelegatedInkPoint",
                       TRACE_EVENT_SCOPE_THREAD, "point", point.ToString());

  pointer_ids_[point.pointer_id()].AddPoint(point);
}

}  // namespace viz
