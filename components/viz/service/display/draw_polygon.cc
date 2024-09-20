// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/draw_polygon.h"

#include <stddef.h>

#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/draw_quad.h"

namespace {
// This threshold controls how "thick" a plane is. If a point's distance is
// <= |split_threshold|, then it is considered on the plane for the purpose of
// polygon splitting.
static const float split_threshold = 0.05f;

static const float normalized_threshold = 0.001f;

void PointInterpolate(const gfx::Point3F& from,
                      const gfx::Point3F& to,
                      double delta,
                      gfx::Point3F* out) {
  out->SetPoint(from.x() + (to.x() - from.x()) * delta,
                from.y() + (to.y() - from.y()) * delta,
                from.z() + (to.z() - from.z()) * delta);
}
}  // namespace

namespace viz {

DrawPolygon::DrawPolygon() = default;

DrawPolygon::DrawPolygon(const DrawQuad* original,
                         const std::vector<gfx::Point3F>& in_points,
                         const gfx::Vector3dF& normal,
                         int draw_order_index)
    : normal_(normal),
      order_index_(draw_order_index),
      original_ref_(original),
      is_split_(true) {
  for (size_t i = 0; i < in_points.size(); i++) {
    points_.push_back(in_points[i]);
  }
  // If life was fair, we could recalculate the normal from the given points
  // and assert it was roughly the same.  This causes unhelpful breaks on
  // trivial slices of split polygons.  Similarly, when splitting, it is
  // better to keep the normal that was constructed from the original.
}

// This takes the original DrawQuad that this polygon should be based on,
// a visible content rect to make the 4 corner points from, and a transformation
// to move it and its normal into screen space.
DrawPolygon::DrawPolygon(const DrawQuad* original_ref,
                         const gfx::RectF& visible_layer_rect,
                         const gfx::Transform& transform,
                         int draw_order_index)
    : normal_(0.0f, 0.0f, 1.0f),
      order_index_(draw_order_index),
      original_ref_(original_ref),
      is_split_(false) {
  std::array<gfx::Point3F, 6> points;
  int num_vertices_in_clipped_quad;
  gfx::QuadF send_quad(visible_layer_rect);

  // Doing this mapping here is very important, since we can't just transform
  // the points without clipping and not run into strange geometry issues when
  // crossing w = 0. At this point, in the constructor, we know that we're
  // working with a quad, so we can reuse the MathUtil::MapClippedQuad3d
  // function instead of writing a generic polygon version of it.
  cc::MathUtil::MapClippedQuad3d(transform, send_quad, points,
                                 &num_vertices_in_clipped_quad);
  for (int i = 0; i < num_vertices_in_clipped_quad; i++) {
    points_.push_back(points[i]);
  }
  normal_ = transform.MapVector(normal_);
  ConstructNormal();
}

DrawPolygon::~DrawPolygon() = default;

std::unique_ptr<DrawPolygon> DrawPolygon::CreateCopy() {
  std::unique_ptr<DrawPolygon> new_polygon(new DrawPolygon());
  new_polygon->order_index_ = order_index_;
  new_polygon->original_ref_ = original_ref_;
  new_polygon->points_.reserve(points_.size());
  new_polygon->points_ = points_;
  new_polygon->normal_.set_x(normal_.x());
  new_polygon->normal_.set_y(normal_.y());
  new_polygon->normal_.set_z(normal_.z());
  return new_polygon;
}

//
// If this were to be more generally used and expected to be applicable
// replacing this with Newell's algorithm (or an improvement thereof)
// would be preferable, but usually this is coming in from a rectangle
// that has been transformed to screen space and clipped.
// Averaging a few near diagonal cross products is pretty good in that case.
//
void DrawPolygon::ConstructNormal() {
  gfx::Vector3dF new_normal(0.0f, 0.0f, 0.0f);
  int delta = points_.size() / 2;
  for (size_t i = 1; i + delta < points_.size(); i++) {
    new_normal +=
        CrossProduct(points_[i] - points_[0], points_[i + delta] - points_[0]);
  }
  float normal_magnitude = new_normal.Length();
  // Here we constrain the new normal to point in the same sense as the old one.
  // This allows us to handle winding-reversing transforms better.
  if (gfx::DotProduct(normal_, new_normal) < 0.0) {
    normal_magnitude *= -1.0;
  }
  if (normal_magnitude != 0 && normal_magnitude != 1) {
    new_normal.Scale(1.0f / normal_magnitude);
  }
  normal_ = new_normal;
}

#if BUILDFLAG(IS_WIN)
//
// Allows the unittest to invoke this for the more general constructor.
//
void DrawPolygon::RecomputeNormalForTesting() {
  ConstructNormal();
}
#endif

float DrawPolygon::SignedPointDistance(const gfx::Point3F& point) const {
  return gfx::DotProduct(point - points_[0], normal_);
}

// This function is separate from ApplyTransform because it is often unnecessary
// to transform the normal with the rest of the polygon.
// When drawing these polygons, it is necessary to move them back into layer
// space before sending them to OpenGL, which requires using ApplyTransform,
// but normal information is no longer needed after sorting.
void DrawPolygon::ApplyTransformToNormal(const gfx::Transform& transform) {
  // Now we use the inverse transpose of |transform| to transform the normal.
  gfx::Transform inverse_transform = transform.GetCheckedInverse();
  inverse_transform.Transpose();
  normal_ = inverse_transform.MapVector(normal_);

  // Make sure our normal is still normalized.
  float normal_magnitude = normal_.Length();
  if (normal_magnitude != 0 && normal_magnitude != 1) {
    normal_.InvScale(normal_magnitude);
  }
}

void DrawPolygon::ApplyTransform(const gfx::Transform& transform) {
  for (auto& p : points_)
    p = transform.MapPoint(p);
}

// TransformToScreenSpace assumes we're moving a layer from its layer space
// into 3D screen space, which for sorting purposes requires the normal to
// be transformed along with the vertices.
void DrawPolygon::TransformToScreenSpace(const gfx::Transform& transform) {
  ApplyTransform(transform);
  normal_ = transform.MapVector(normal_);
  ConstructNormal();
}

// In the case of TransformToLayerSpace, we assume that we are giving the
// inverse transformation back to the polygon to move it back into layer space
// but we can ignore the costly process of applying the inverse to the normal
// since we know the normal will just reset to its original state.
void DrawPolygon::TransformToLayerSpace(
    const gfx::Transform& inverse_transform) {
  ApplyTransform(inverse_transform);
  normal_ = gfx::Vector3dF(0.0f, 0.0f, -1.0f);
}

// Split |polygon| based upon |this|, leaving the results in |front| and |back|.
// If |polygon| is not split by |this|, then move it to either |front| or |back|
// depending on its orientation relative to |this|. Sets |is_coplanar| to true
// if |polygon| is actually coplanar with |this| (in which case whether it is
// front facing or back facing is determined by the dot products of normals, and
// document order).
void DrawPolygon::SplitPolygon(std::unique_ptr<DrawPolygon> polygon,
                               std::unique_ptr<DrawPolygon>* front,
                               std::unique_ptr<DrawPolygon>* back,
                               bool* is_coplanar) const {
  DCHECK_GE(normalized_threshold, std::abs(normal_.LengthSquared() - 1.0f));

  const size_t num_points = polygon->points_.size();
  const auto next = [num_points](size_t i) { return (i + 1) % num_points; };
  const auto prev = [num_points](size_t i) {
    return (i + num_points - 1) % num_points;
  };

  std::vector<float> vertex_distance;
  size_t pos_count = 0;
  size_t neg_count = 0;

  // Compute plane distances for each vertex of polygon.
  vertex_distance.resize(num_points);
  for (size_t i = 0; i < num_points; i++) {
    vertex_distance[i] = SignedPointDistance(polygon->points_[i]);
    if (vertex_distance[i] < -split_threshold) {
      ++neg_count;
    } else if (vertex_distance[i] > split_threshold) {
      ++pos_count;
    } else {
      vertex_distance[i] = 0.0;
    }
  }

  // Handle non-splitting cases.
  if (!pos_count && !neg_count) {
    if (polygon->order_index_ >= order_index_) {
      *front = std::move(polygon);
    } else {
      *back = std::move(polygon);
    }
    *is_coplanar = true;
    return;
  }

  *is_coplanar = false;
  if (!neg_count) {
    *front = std::move(polygon);
    return;
  } else if (!pos_count) {
    *back = std::move(polygon);
    return;
  }

  // Handle splitting case.
  size_t front_begin;
  size_t back_begin;
  size_t pre_front_begin;
  size_t pre_back_begin;

  // Find the first vertex that is part of the front split polygon.
  front_begin = base::ranges::find_if(vertex_distance,
                                      [](float val) { return val > 0.0; }) -
                vertex_distance.begin();
  while (vertex_distance[pre_front_begin = prev(front_begin)] > 0.0)
    front_begin = pre_front_begin;

  // Find the first vertex that is part of the back split polygon.
  back_begin = base::ranges::find_if(vertex_distance,
                                     [](float val) { return val < 0.0; }) -
               vertex_distance.begin();
  while (vertex_distance[pre_back_begin = prev(back_begin)] < 0.0)
    back_begin = pre_back_begin;

  DCHECK(vertex_distance[front_begin] > 0.0);
  DCHECK(vertex_distance[pre_front_begin] <= 0.0);
  DCHECK(vertex_distance[back_begin] < 0.0);
  DCHECK(vertex_distance[pre_back_begin] >= 0.0);

  gfx::Point3F pre_pos_intersection;
  gfx::Point3F pre_neg_intersection;

  // Compute the intersection points. N.B.: If the "pre" vertex is on
  // the thick plane, then the intersection will be at the same point, because
  // we set vertex_distance to 0 in this case.
  //
  // Consistently use the vertex distances rather than taking dot
  // products with the normal, because there may be some amount of
  // accumulated floating point error in the coplanarity of the points.
  // The vertex distances are what we used to separate front from back,
  // so using them guarantees a nonzero denominator.
  PointInterpolate(
      polygon->points_[pre_front_begin], polygon->points_[front_begin],
      vertex_distance[pre_front_begin] /
          (vertex_distance[pre_front_begin] - vertex_distance[front_begin]),
      &pre_pos_intersection);
  DCHECK(std::isfinite(pre_pos_intersection.x()));
  DCHECK(std::isfinite(pre_pos_intersection.y()));
  DCHECK(std::isfinite(pre_pos_intersection.z()));
  PointInterpolate(
      polygon->points_[pre_back_begin], polygon->points_[back_begin],
      vertex_distance[pre_back_begin] /
          (vertex_distance[pre_back_begin] - vertex_distance[back_begin]),
      &pre_neg_intersection);
  DCHECK(std::isfinite(pre_neg_intersection.x()));
  DCHECK(std::isfinite(pre_neg_intersection.y()));
  DCHECK(std::isfinite(pre_neg_intersection.z()));

  // Build the front and back polygons.
  std::vector<gfx::Point3F> out_points;

  out_points.push_back(pre_pos_intersection);
  for (size_t index = front_begin; index != back_begin; index = next(index)) {
    out_points.push_back(polygon->points_[index]);
  }
  if (out_points.back() != pre_neg_intersection) {
    out_points.push_back(pre_neg_intersection);
  }
  *front =
      std::make_unique<DrawPolygon>(polygon->original_ref_, out_points,
                                    polygon->normal_, polygon->order_index_);

  out_points.clear();

  out_points.push_back(pre_neg_intersection);
  for (size_t index = back_begin; index != front_begin; index = next(index)) {
    out_points.push_back(polygon->points_[index]);
  }
  if (out_points.back() != pre_pos_intersection) {
    out_points.push_back(pre_pos_intersection);
  }
  *back =
      std::make_unique<DrawPolygon>(polygon->original_ref_, out_points,
                                    polygon->normal_, polygon->order_index_);

  DCHECK_GE((*front)->points().size(), 3u);
  DCHECK_GE((*back)->points().size(), 3u);
}

// This algorithm takes the first vertex in the polygon and uses that as a
// pivot point to fan out and create quads from the rest of the vertices.
// |offset| starts off as the second vertex, and then |op1| and |op2| indicate
// offset+1 and offset+2 respectively.
// After the first quad is created, the first vertex in the next quad is the
// same as all the rest, the pivot point. The second vertex in the next quad is
// the old |op2|, the last vertex added to the previous quad. This continues
// until all points are exhausted.
// The special case here is where there are only 3 points remaining, in which
// case we use the same values for vertex 3 and 4 to make a degenerate quad
// that represents a triangle.
void DrawPolygon::ToQuads2D(std::vector<gfx::QuadF>* quads) const {
  if (points_.size() <= 2)
    return;

  gfx::PointF first(points_[0].x(), points_[0].y());
  size_t offset = 1;
  while (offset < points_.size() - 1) {
    size_t op1 = offset + 1;
    size_t op2 = offset + 2;
    if (op2 >= points_.size()) {
      // It's going to be a degenerate triangle.
      op2 = op1;
    }
    quads->push_back(
        gfx::QuadF(first, gfx::PointF(points_[offset].x(), points_[offset].y()),
                   gfx::PointF(points_[op1].x(), points_[op1].y()),
                   gfx::PointF(points_[op2].x(), points_[op2].y())));
    offset = op2;
  }
}

}  // namespace viz
