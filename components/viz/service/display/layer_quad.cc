// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/layer_quad.h"

#include <stddef.h>

#include "base/check.h"
#include "ui/gfx/geometry/quad_f.h"

namespace viz {

LayerQuad::Edge::Edge(const gfx::PointF& p, const gfx::PointF& q) {
  if (p == q) {
    degenerate_ = true;
    return;
  }
  degenerate_ = false;
  gfx::Vector2dF tangent(p.y() - q.y(), q.x() - p.x());
  float cross2 = p.x() * q.y() - q.x() * p.y();

  set(tangent.x(), tangent.y(), cross2);
  scale(1.0f / tangent.Length());
}

gfx::PointF LayerQuad::Edge::Intersect(const LayerQuad::Edge& e) const {
  DCHECK(!degenerate());
  DCHECK(!e.degenerate());

  return gfx::PointF((y() * e.z() - e.y() * z()) / (x() * e.y() - e.x() * y()),
                     (x() * e.z() - e.x() * z()) / (e.x() * y() - x() * e.y()));
}

LayerQuad::LayerQuad(const gfx::QuadF& quad) {
  // Create edges.
  left_ = Edge(quad.p4(), quad.p1());
  right_ = Edge(quad.p2(), quad.p3());
  top_ = Edge(quad.p1(), quad.p2());
  bottom_ = Edge(quad.p3(), quad.p4());

  float sign = quad.IsCounterClockwise() ? -1 : 1;
  left_.scale(sign);
  right_.scale(sign);
  top_.scale(sign);
  bottom_.scale(sign);
}

LayerQuad::LayerQuad(const Edge& left,
                     const Edge& top,
                     const Edge& right,
                     const Edge& bottom)
    : left_(left), top_(top), right_(right), bottom_(bottom) {}

gfx::QuadF LayerQuad::ToQuadF() const {
  size_t num_degenerate_edges = left_.degenerate() + right_.degenerate() +
                                top_.degenerate() + bottom_.degenerate();
  if (num_degenerate_edges > 1) {
    return gfx::QuadF();
  }

  if (left_.degenerate()) {
    return gfx::QuadF(top_.Intersect(bottom_), top_.Intersect(right_),
                      right_.Intersect(bottom_), bottom_.Intersect(top_));
  }
  if (right_.degenerate()) {
    return gfx::QuadF(left_.Intersect(top_), top_.Intersect(bottom_),
                      bottom_.Intersect(top_), bottom_.Intersect(left_));
  }
  if (top_.degenerate()) {
    return gfx::QuadF(left_.Intersect(right_), right_.Intersect(left_),
                      right_.Intersect(bottom_), bottom_.Intersect(left_));
  }
  if (bottom_.degenerate()) {
    return gfx::QuadF(left_.Intersect(top_), top_.Intersect(right_),
                      right_.Intersect(left_), left_.Intersect(right_));
  }
  return gfx::QuadF(left_.Intersect(top_), top_.Intersect(right_),
                    right_.Intersect(bottom_), bottom_.Intersect(left_));
}

void LayerQuad::ToFloatArray(float flattened[12]) const {
  if (left_.degenerate()) {
    flattened[0] = bottom_.x();
    flattened[1] = bottom_.y();
    flattened[2] = bottom_.z();
  } else {
    flattened[0] = left_.x();
    flattened[1] = left_.y();
    flattened[2] = left_.z();
  }
  if (top_.degenerate()) {
    flattened[3] = left_.x();
    flattened[4] = left_.y();
    flattened[5] = left_.z();
  } else {
    flattened[3] = top_.x();
    flattened[4] = top_.y();
    flattened[5] = top_.z();
  }
  if (right_.degenerate()) {
    flattened[6] = top_.x();
    flattened[7] = top_.y();
    flattened[8] = top_.z();
  } else {
    flattened[6] = right_.x();
    flattened[7] = right_.y();
    flattened[8] = right_.z();
  }
  if (bottom_.degenerate()) {
    flattened[9] = right_.x();
    flattened[10] = right_.y();
    flattened[11] = right_.z();
  } else {
    flattened[9] = bottom_.x();
    flattened[10] = bottom_.y();
    flattened[11] = bottom_.z();
  }
}

}  // namespace viz
