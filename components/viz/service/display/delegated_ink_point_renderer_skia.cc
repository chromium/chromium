// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_renderer_skia.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace viz {

void DelegatedInkPointRendererSkia::DrawDelegatedInkTrail(
    SkCanvas* canvas,
    const gfx::Transform& transform_to_render_pass) {
  TRACE_EVENT1("viz", "DelegatedInkPointRendererSkia::DrawDelegatedInkTrail",
               "points", path_.countPoints());

  if (!metadata_) {
    ResetPoints();
    return;
  }

  if (!path_.isEmpty() && canvas) {
    canvas->save();
    canvas->concat(gfx::TransformToSkM44(transform_to_render_pass));

    SkRect bounds = gfx::RectFToSkRect(metadata_->presentation_area());
    canvas->clipRect(bounds);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setBlendMode(SkBlendMode::kSrcOver);
    paint.setColor(metadata_->color());
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeJoin(SkPaint::kRound_Join);
    paint.setStrokeWidth(SkScalar(metadata_->diameter()));
    paint.setStyle(SkPaint::kStroke_Style);

    canvas->drawPath(path_, paint);

    canvas->restore();

    path_.rewind();
  }

  // Always reset `metadata_` regardless of if the draw occurred or not so that
  // the trail is never incorrectly drawn if the aggregated frame did not
  // contain delegated ink metadata.
  metadata_.reset();
}

gfx::Rect DelegatedInkPointRendererSkia::GetDamageRect() {
  if (old_trail_damage_rect_.IsEmpty() && new_trail_damage_rect_.IsEmpty())
    return gfx::Rect();

  gfx::RectF damage_rect_f = old_trail_damage_rect_;

  damage_rect_f.Union(new_trail_damage_rect_);

  return gfx::ToEnclosingRect(damage_rect_f);
}

base::TimeDelta GetImprovement(
    const std::vector<gfx::DelegatedInkPoint>* points_to_draw,
    const gfx::DelegatedInkMetadata* metadata) {
  if (points_to_draw->size() == 0)
    return base::Milliseconds(0);

  return points_to_draw->back().timestamp() - metadata->timestamp();
}

std::vector<SkPoint> DelegatedInkPointRendererSkia::GetPointsToDraw() {
  std::vector<gfx::DelegatedInkPoint> ink_points_to_draw = FilterPoints();
  UMA_HISTOGRAM_TIMES(
      "Renderer.DelegatedInkTrail.LatencyImprovement.Skia.WithoutPrediction",
      GetImprovement(&ink_points_to_draw, metadata_.get()));

  PredictPoints(&ink_points_to_draw);

  std::vector<SkPoint> sk_points;
  for (gfx::DelegatedInkPoint ink_point : ink_points_to_draw) {
    sk_points.push_back(gfx::PointFToSkPoint(ink_point.point()));
  }

  return sk_points;
}

void DelegatedInkPointRendererSkia::FinalizePathForDraw() {
  // Always rewind the path first so that a path isn't drawn twice.
  path_.rewind();

  // Setting the damage rect to empty ensures that the damage rect is cleared
  // when trails are not being drawn so that extra drawing doesn't occur. If
  // there isn't metadata, that also indicates that the previous trail has
  // finished so the predictor should be reset as well.
  if (!metadata_) {
    SetDamageRect(gfx::RectF());
    ResetPrediction();
    ResetPoints();
    return;
  }

  std::vector<SkPoint> sk_points = GetPointsToDraw();

  TRACE_EVENT_INSTANT1("delegated_ink_trails",
                       "Filtered and predicted points for delegated ink trail",
                       TRACE_EVENT_SCOPE_THREAD, "points", sk_points.size());

  // If there is only one point total after filtering and predicting, then it
  // will match the metadata point and therefore doesn't need to be drawn in
  // this way, as it will be rendered normally.
  if (sk_points.size() <= 1) {
    SetDamageRect(gfx::RectF());
    return;
  }

  path_.moveTo(sk_points[0]);
  switch (sk_points.size()) {
    case 2:
      path_.lineTo(sk_points[1]);
      break;
    case 3:
      path_.quadTo(sk_points[1], sk_points[2]);
      break;
    case 4:
      path_.cubicTo(sk_points[1], sk_points[2], sk_points[3]);
      break;
    default:
      // The connection between two cubic bezier curves will be smooth only if
      // the second control point of the first curve, the end point of the first
      // curve/first control point of the second curve, and the second control
      // point of the second curve are colinear. Since this is unlikely to be
      // the case, connecting all four points via lines should be acceptable.
      for (uint64_t i = 1; i < sk_points.size(); ++i)
        path_.lineTo(sk_points[i]);
      break;
  }

  // path_.computeTightBounds() returns a rect that contains the points and
  // curves, but it isn't guaranteed to contain the drawn stroke, resulting in
  // the stroke sometimes existing outside of the damage_rect. Therefore, expand
  // it here to ensure that the stroke is included, then intersect with the
  // presentation area so that is can't extend beyond the drawable area.
  gfx::RectF damage_rect = gfx::SkRectToRectF(path_.computeTightBounds());
  const float kRadius = metadata_->diameter() / 2.f;
  damage_rect.Inset(-kRadius);
  damage_rect.Intersect(metadata_->presentation_area());

  TRACE_EVENT_INSTANT1("delegated_ink_trails",
                       "DelegatedInkPointRendererSkia::FinalizePathForDraw",
                       TRACE_EVENT_SCOPE_THREAD, "damage_rect",
                       damage_rect.ToString());

  SetDamageRect(damage_rect);
}

void DelegatedInkPointRendererSkia::SetDamageRect(gfx::RectF damage_rect) {
  old_trail_damage_rect_ = new_trail_damage_rect_;
  new_trail_damage_rect_ = damage_rect;
}

int DelegatedInkPointRendererSkia::GetPathPointCountForTest() const {
  return path_.countPoints();
}

}  // namespace viz
