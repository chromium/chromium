// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DRAW_POLYGON_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DRAW_POLYGON_H_

#include <memory>
#include <vector>

#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

namespace viz {
class DrawQuad;

class VIZ_SERVICE_EXPORT DrawPolygon {
 public:
  DrawPolygon();
  ~DrawPolygon();

  DrawPolygon(const DrawQuad* original_ref,
              const std::vector<gfx::Point3F>& in_points,
              const gfx::Vector3dF& normal,
              int draw_order_index = 0);
  DrawPolygon(const DrawQuad* original_ref,
              const gfx::RectF& visible_layer_rect,
              const gfx::Transform& transform,
              int draw_order_index = 0);

  // Split takes this DrawPolygon and splits it into two pieces that are on
  // either side of |splitter|. Any edges of this polygon that cross the plane
  // of |splitter| will have an intersection point that is shared by both
  // polygons on either side.
  // Split will only return true if it determines that we got back 2
  // intersection points. Only when it returns true will front and back both be
  // valid new polygons that are on opposite sides of the splitting plane.
  void SplitPolygon(std::unique_ptr<DrawPolygon> polygon,
                    std::unique_ptr<DrawPolygon>* front,
                    std::unique_ptr<DrawPolygon>* back,
                    bool* is_coplanar) const;
  float SignedPointDistance(const gfx::Point3F& point) const;
  void ToQuads2D(std::vector<gfx::QuadF>* quads) const;
  void TransformToScreenSpace(const gfx::Transform& transform);
  void TransformToLayerSpace(const gfx::Transform& inverse_transform);

  const std::vector<gfx::Point3F>& points() const { return points_; }
  const gfx::Vector3dF& normal() const { return normal_; }
  const DrawQuad* original_ref() const { return original_ref_; }
  int order_index() const { return order_index_; }
  bool is_split() const { return is_split_; }
  std::unique_ptr<DrawPolygon> CreateCopy();

  // These are helper functions for testing.
  void RecomputeNormalForTesting();
  friend bool IsPlanarForTesting(const DrawPolygon& p);
  friend bool IsConvexForTesting(const DrawPolygon& p);

 private:
  void ApplyTransform(const gfx::Transform& transform);
  void ApplyTransformToNormal(const gfx::Transform& transform);

  void ConstructNormal();

  std::vector<gfx::Point3F> points_;
  // Normalized, necessitated by distance calculations and tests of coplanarity.
  gfx::Vector3dF normal_;
  // This is an index that can be used to test whether a quad comes before or
  // after another in document order, useful for tie-breaking when it comes
  // to coplanar surfaces.
  int order_index_;
  // The pointer to the original quad, which gives us all the drawing info
  // we need.
  // This DrawQuad is owned by the caller and its lifetime must be preserved
  // as long as this DrawPolygon is alive.
  const DrawQuad* original_ref_;
  bool is_split_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DRAW_POLYGON_H_
