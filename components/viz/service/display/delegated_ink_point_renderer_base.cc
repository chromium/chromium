// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_renderer_base.h"

#include <algorithm>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/service/display/delegated_ink_trail_data.h"

namespace viz {

DelegatedInkPointRendererBase::DelegatedInkPointRendererBase() = default;
DelegatedInkPointRendererBase::~DelegatedInkPointRendererBase() = default;

void DelegatedInkPointRendererBase::InitMessagePipeline(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver) {
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
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  // Frame time is set later than everything else due to what is available
  // at time of creation, so confirm that it was actually set.
  CHECK_NE(metadata->frame_time(), base::TimeTicks());
  metadata_ = std::move(metadata);
  const bool metadata_is_new = previous_metadata_ != *metadata_;
  previous_metadata_ = *metadata_;

  TRACE_EVENT_WITH_FLOW1(
      "delegated_ink_trails",
      "DelegatedInkPointRendererBase::SetDelegatedInkMetadata",
      TRACE_ID_GLOBAL(metadata_->trace_id()), TRACE_EVENT_FLAG_FLOW_IN,
      "metadata", metadata_->ToString());

  // If we already have a cached pointer ID, check if the same pointer ID
  // matches the new metadata.
  if (pointer_id_.has_value() &&
      pointer_ids_[pointer_id_.value()].ContainsMatchingPoint(
          metadata_.get())) {
    if (metadata_is_new) {
      metadata_paint_time_ = pointer_ids_[pointer_id_.value()]
                                 .GetMatchingPoint(metadata_.get())
                                 .paint_timestamp();
    }
    return;
  }

  // If not, find the pointer ID that does match it, if any, and cache it.
  for (auto& it : pointer_ids_) {
    if (it.second.ContainsMatchingPoint(metadata_.get())) {
      pointer_id_ = it.first;
      metadata_paint_time_ =
          it.second.GetMatchingPoint(metadata_.get()).paint_timestamp();
      return;
    }
  }

  // If we aren't able to find any matching point, set the pointer ID to null
  // so that FilterPoints and PredictPoints can early out.
  pointer_id_ = std::nullopt;
}

void DelegatedInkPointRendererBase::ResetPoints() {
  CHECK(!metadata_);
  pointer_ids_.clear();
  pointer_id_.reset();
}

std::vector<gfx::DelegatedInkPoint>
DelegatedInkPointRendererBase::FilterPoints() {
  if (pointer_ids_.empty()) {
    return {};
  }

  CHECK(metadata_);

  // Any stored point with a timestamp earlier than the metadata's has already
  // been drawn as part of the ink stroke and therefore should not be part of
  // the delegated ink trail. Do this before checking if |pointer_id_| is valid
  // because it helps manage the number of DelegatedInkPoints that are being
  // stored and isn't dependent on |pointer_id_| at all.
  for (auto& it : pointer_ids_) {
    it.second.ErasePointsOlderThanMetadata(metadata_.get());
  }

  // TODO(crbug.com/40118757): Add additional filtering to prevent points in
  // |points_| from having a timestamp that is far ahead of |metadata_|'s
  // timestamp. This could occur if the renderer stalls before sending a
  // metadata while the browser continues to pump points through to viz. Then
  // when the renderer starts back up again, the metadata it sends may be
  // significantly older than the points stored here, resulting in a long
  // possibly incorrect trail if the max number of points to store was reached.

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
  if (!pointer_id_.has_value()) {
    return {};
  }

  DelegatedInkTrailData& trail_data = pointer_ids_[pointer_id_.value()];

  // Make sure the metrics handler is provided the new real events to accurately
  // measure the prediction later.
  trail_data.UpdateMetrics(metadata_.get());

  // Any remaining points must be the points that should be part of the
  // delegated ink trail
  std::vector<gfx::DelegatedInkPoint> points_to_draw;
  for (const auto& [_, point] : trail_data.GetPoints()) {
    points_to_draw.emplace_back(point);
    TRACE_EVENT_WITH_FLOW1("delegated_ink_trails", "Filtering to draw point",
                           TRACE_ID_GLOBAL(point.trace_id()),
                           TRACE_EVENT_FLAG_FLOW_IN, "point", point.ToString());
  }

  if (!points_to_draw.front().MatchesDelegatedInkMetadata(metadata_.get())) {
    base::debug::DumpWithoutCrashing();
  }

  return points_to_draw;
}

std::optional<AggregatedRenderPassId>
DelegatedInkPointRendererBase::GetLatestMetadataRenderPassId() const {
  if (metadata_) {
    return AggregatedRenderPassId::FromUnsafeValue(metadata_->render_pass_id());
  }
  return std::nullopt;
}

void DelegatedInkPointRendererBase::PredictPoints(
    std::vector<gfx::DelegatedInkPoint>* ink_points_to_draw) {
  DCHECK(metadata_);

  if (!pointer_id_.has_value() ||
      static_cast<int>(ink_points_to_draw->size()) == 0)
    return;

  pointer_ids_[pointer_id_.value()].PredictPoints(ink_points_to_draw,
                                                  metadata_.get());
}

void DelegatedInkPointRendererBase::ResetPrediction() {
  for (auto& it : pointer_ids_)
    it.second.Reset();
  TRACE_EVENT_INSTANT0("delegated_ink_trails",
                       "Delegated ink prediction reset.",
                       TRACE_EVENT_SCOPE_THREAD);
}

void DelegatedInkPointRendererBase::ReportPointsDrawn() {
  const base::TimeTicks now = base::TimeTicks::Now();
  // If there is a point that matches the metadata and the histogram has not yet
  // been fired, then this is the first frame that the metadata point will be
  // recorded by `updateInkTrailStartPoint`.
  if (metadata_paint_time_.has_value()) {
    base::UmaHistogramCustomTimes(
        "Renderer.DelegatedInkTrail.Skia.TimeFromDelegatedInkToApiPaint",
        now - metadata_paint_time_.value(), base::Milliseconds(1),
        base::Seconds(1), 50);
    metadata_paint_time_ = std::nullopt;
  }

  if (!pointer_id_.has_value()) {
    return;
  }
  CHECK(pointer_ids_.contains(pointer_id_.value()));
  auto& points_trail = pointer_ids_.at(pointer_id_.value()).GetPoints();
  base::UmaHistogramCounts100(
      "Renderer.DelegatedInkTrail.Skia.OutstandingPointsToDraw",
      points_trail.size());

  for (auto& [_, point] : points_trail) {
    UMA_HISTOGRAM_TIMES(
        "Renderer.DelegatedInkTrail.Skia.TimeToDrawPointsMillis",
        now - point.timestamp());
    if (!point.paint_timestamp().has_value()) {
      point.set_paint_timestamp(now);
    }
  }
}

void DelegatedInkPointRendererBase::StoreDelegatedInkPoint(
    const gfx::DelegatedInkPoint& point) {
  TRACE_EVENT_WITH_FLOW1(
      "delegated_ink_trails",
      "DelegatedInkPointRendererImpl::StoreDelegatedInkPoint",
      TRACE_ID_GLOBAL(point.trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "point",
      point.ToString());

  pointer_ids_[point.pointer_id()].AddPoint(point);
}

}  // namespace viz
