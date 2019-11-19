// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <limits>
#include <vector>

#include "base/numerics/math_constants.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/viz/service/display/bsp_compare_result.h"
#include "components/viz/service/display/draw_polygon.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace viz {

#if !defined(OS_WIN)
void DrawPolygon::RecomputeNormalForTesting() {
  ConstructNormal();
}
#endif

static int sign(float v) {
  static const float epsilon = 0.00001f;

  if (v > epsilon)
    return 1;
  if (v < -epsilon)
    return -1;
  return 0;
}

bool IsPlanarForTesting(const DrawPolygon& p) {
  static const float epsilon = 0.00001f;
  for (size_t i = 1; i < p.points_.size(); i++) {
    if (gfx::DotProduct(p.points_[i] - p.points_[0], p.normal_) > epsilon)
      return false;
  }
  return true;
}

bool IsConvexForTesting(const DrawPolygon& p) {
  if (p.points_.size() < 3)
    return true;

  gfx::Vector3dF prev =
      p.points_[p.points_.size() - 1] - p.points_[p.points_.size() - 2];
  gfx::Vector3dF next = p.points_[0] - p.points_[p.points_.size() - 1];
  int ccw = sign(gfx::DotProduct(CrossProduct(prev, next), p.normal_));
  for (size_t i = 1; i < p.points_.size(); i++) {
    prev = next;
    next = p.points_[i] - p.points_[i - 1];
    int next_sign = sign(gfx::DotProduct(CrossProduct(prev, next), p.normal_));
    if (ccw == 0)
      ccw = next_sign;
    if (next_sign != 0 && next_sign != ccw)
      return false;
  }
  return true;
}

namespace {

#define CREATE_NEW_DRAW_POLYGON(name, points_vector, normal, polygon_id) \
  DrawPolygon name(NULL, points_vector, normal, polygon_id)

#define CREATE_NEW_DRAW_POLYGON_PTR(name, points_vector, normal, polygon_id) \
  std::unique_ptr<DrawPolygon> name(std::make_unique<DrawPolygon>(           \
      nullptr, points_vector, normal, polygon_id))

#define CREATE_TEST_DRAW_FORWARD_POLYGON(name, points_vector, id)        \
  DrawPolygon name(NULL, points_vector, gfx::Vector3dF(0, 0, 1.0f), id); \
  name.RecomputeNormalForTesting()

#define CREATE_TEST_DRAW_REVERSE_POLYGON(name, points_vector, id)         \
  DrawPolygon name(NULL, points_vector, gfx::Vector3dF(0, 0, -1.0f), id); \
  name.RecomputeNormalForTesting()

#define EXPECT_FLOAT_WITHIN_EPSILON_OF(a, b)                               \
  LOG(WARNING) << "a=" << a << " b= " << b << " diff=" << std::abs(a - b); \
  EXPECT_TRUE(std::abs(a - b) < std::numeric_limits<float>::epsilon());

#define EXPECT_POINT_EQ(point_a, point_b)    \
  EXPECT_FLOAT_EQ(point_a.x(), point_b.x()); \
  EXPECT_FLOAT_EQ(point_a.y(), point_b.y()); \
  EXPECT_FLOAT_EQ(point_a.z(), point_b.z());

#define EXPECT_NORMAL(poly, n_x, n_y, n_z)                \
  EXPECT_FLOAT_WITHIN_EPSILON_OF(poly.normal().x(), n_x); \
  EXPECT_FLOAT_WITHIN_EPSILON_OF(poly.normal().y(), n_y); \
  EXPECT_FLOAT_WITHIN_EPSILON_OF(poly.normal().z(), n_z);

static void ValidatePoints(const DrawPolygon& polygon,
                           const std::vector<gfx::Point3F>& points) {
  EXPECT_EQ(polygon.points().size(), points.size());
  for (size_t i = 0; i < points.size(); i++) {
    EXPECT_POINT_EQ(polygon.points()[i], points[i]);
  }
}

static void ValidatePointsWithinDeltaOf(const DrawPolygon& polygon,
                                        const std::vector<gfx::Point3F>& points,
                                        float delta) {
  EXPECT_EQ(polygon.points().size(), points.size());
  for (size_t i = 0; i < points.size(); i++) {
    EXPECT_LE((polygon.points()[i] - points[i]).Length(), delta);
  }
}

// A simple square in a plane.
TEST(DrawPolygonConstructionTest, NormalNormal) {
  gfx::Transform Identity;
  DrawPolygon polygon(nullptr, gfx::RectF(10.0f, 10.0f), Identity, 1);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

// More complicated shapes.
TEST(DrawPolygonConstructionTest, TestNormal) {
  std::vector<gfx::Point3F> vertices;
  vertices.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));

  CREATE_TEST_DRAW_FORWARD_POLYGON(polygon, vertices, 1);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, ClippedNormal) {
  std::vector<gfx::Point3F> vertices;
  vertices.push_back(gfx::Point3F(0.1f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 9.9f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));

  CREATE_TEST_DRAW_FORWARD_POLYGON(polygon, vertices, 1);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, SlimTriangleNormal) {
  std::vector<gfx::Point3F> vertices;
  vertices.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(5000.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10000.0f, 1.0f, 0.0f));

  CREATE_TEST_DRAW_FORWARD_POLYGON(polygon, vertices, 2);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, ManyVertexNormal) {
  std::vector<gfx::Point3F> vertices_c;
  std::vector<gfx::Point3F> vertices_d;
  for (int i = 0; i < 100; i++) {
    const double step = i * base::kPiDouble / 50;
    vertices_c.push_back(gfx::Point3F(cos(step), sin(step), 0.0f));
    vertices_d.push_back(
        gfx::Point3F(cos(step) + 99.0f, sin(step) + 99.0f, 100.0f));
  }
  CREATE_TEST_DRAW_FORWARD_POLYGON(polygon_c, vertices_c, 3);
  EXPECT_NORMAL(polygon_c, 0.0f, 0.0f, 1.0f);

  CREATE_TEST_DRAW_FORWARD_POLYGON(polygon_d, vertices_d, 4);
  EXPECT_NORMAL(polygon_d, 0.0f, 0.0f, 1.0f);
}

// A simple rect being transformed.
TEST(DrawPolygonConstructionTest, SimpleNormal) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform_i(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_i(nullptr, src, transform_i, 1);

  EXPECT_NORMAL(polygon_i, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, DISABLED_NormalInvertXY) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform(0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_a(nullptr, src, transform, 2);

  EXPECT_NORMAL(polygon_a, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, DISABLED_NormalInvertXZ) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform(0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1);
  DrawPolygon polygon_b(nullptr, src, transform, 3);

  EXPECT_NORMAL(polygon_b, 1.0f, 0.0f, 0.0f);
}

TEST(DrawPolygonConstructionTest, DISABLED_NormalInvertYZ) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform(1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1);
  DrawPolygon polygon_c(nullptr, src, transform, 4);

  EXPECT_NORMAL(polygon_c, 0.0f, 1.0f, 0.0f);
}

TEST(DrawPolygonConstructionTest, NormalRotate90) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform(0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1);
  DrawPolygon polygon_b(nullptr, src, transform, 3);

  EXPECT_NORMAL(polygon_b, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, InvertXNormal) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform(-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_d(nullptr, src, transform, 5);

  EXPECT_NORMAL(polygon_d, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, InvertYNormal) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_d(nullptr, src, transform, 5);

  EXPECT_NORMAL(polygon_d, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, InvertZNormal) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_d(nullptr, src, transform, 5);

  EXPECT_NORMAL(polygon_d, 0.0f, 0.0f, -1.0f);
}

// Two quads are nearly touching but definitely ordered. Second one should
// compare in front.
TEST(DrawPolygonSplitTest, NearlyTouchingOrder) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(0.0f, 10.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(0.0f, 0.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(10.0f, 0.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(10.0f, 10.0f, -1.0f));
  gfx::Vector3dF normal(0.0f, 0.0f, 1.0f);

  CREATE_NEW_DRAW_POLYGON(polygon_a, vertices_a, normal, 0);
  CREATE_NEW_DRAW_POLYGON_PTR(polygon_b, vertices_b, normal, 1);

  std::unique_ptr<DrawPolygon> front;
  std::unique_ptr<DrawPolygon> back;
  bool is_coplanar;
  polygon_a.SplitPolygon(std::move(polygon_b), &front, &back, &is_coplanar);
  EXPECT_EQ(is_coplanar, false);
  EXPECT_EQ(front, nullptr);
  EXPECT_NE(back, nullptr);
}

// Two quads are definitely not touching and so no split should occur.
TEST(DrawPolygonSplitTest, NotClearlyInFront) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(87.2f, 1185.0f, 0.9f));
  vertices_a.push_back(gfx::Point3F(288.3f, 1185.0f, -0.7f));
  vertices_a.push_back(gfx::Point3F(288.3f, 1196.0f, -0.7f));
  vertices_a.push_back(gfx::Point3F(87.2f, 1196.0f, 0.9f));
  gfx::Vector3dF normal_a = gfx::CrossProduct(vertices_a[1] - vertices_a[0],
                                              vertices_a[1] - vertices_a[2]);
  normal_a.Scale(1.0f / normal_a.Length());

  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(62.1f, 1034.7f, 1.0f));
  vertices_b.push_back(gfx::Point3F(313.4f, 1035.3f, -1.0f));
  vertices_b.push_back(gfx::Point3F(313.4f, 1196.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(62.1f, 1196.0f, 1.0f));
  gfx::Vector3dF normal_b = gfx::CrossProduct(vertices_b[1] - vertices_b[0],
                                              vertices_b[1] - vertices_b[2]);
  normal_b.Scale(1.0f / normal_b.Length());

  CREATE_NEW_DRAW_POLYGON(polygon_a, vertices_a, normal_a, 0);
  CREATE_NEW_DRAW_POLYGON_PTR(polygon_b, vertices_b, normal_b, 1);

  std::unique_ptr<DrawPolygon> front;
  std::unique_ptr<DrawPolygon> back;
  bool is_coplanar;
  polygon_a.SplitPolygon(std::move(polygon_b), &front, &back, &is_coplanar);
  EXPECT_EQ(is_coplanar, false);
  EXPECT_NE(front, nullptr);
  EXPECT_EQ(back, nullptr);
}

// Two quads are definitely not touching and so no split should occur.
TEST(DrawPolygonSplitTest, NotTouchingNoSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 15.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, 15.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, 5.0f));

  CREATE_NEW_DRAW_POLYGON(polygon_a, vertices_a,
                          gfx::Vector3dF(0.0f, 0.0f, 1.0f), 0);
  CREATE_NEW_DRAW_POLYGON_PTR(polygon_b, vertices_b,
                              gfx::Vector3dF(-1.0f, 0.0f, 0.0f), 1);

  std::unique_ptr<DrawPolygon> front;
  std::unique_ptr<DrawPolygon> back;
  bool is_coplanar;
  polygon_a.SplitPolygon(std::move(polygon_b), &front, &back, &is_coplanar);
  EXPECT_EQ(is_coplanar, false);
  EXPECT_NE(front, nullptr);
  EXPECT_EQ(back, nullptr);
}

// One quad is resting against another, but doesn't cross its plane so no
// split
// should occur.
TEST(DrawPolygonSplitTest, BarelyTouchingNoSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 0.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, -10.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, -10.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, 0.0f));

  CREATE_NEW_DRAW_POLYGON(polygon_a, vertices_a,
                          gfx::Vector3dF(0.0f, 0.0f, 1.0f), 0);
  CREATE_NEW_DRAW_POLYGON_PTR(polygon_b, vertices_b,
                              gfx::Vector3dF(-1.0f, 0.0f, 0.0f), 1);

  std::unique_ptr<DrawPolygon> front;
  std::unique_ptr<DrawPolygon> back;
  bool is_coplanar;
  polygon_a.SplitPolygon(std::move(polygon_b), &front, &back, &is_coplanar);
  EXPECT_EQ(is_coplanar, false);
  EXPECT_EQ(front, nullptr);
  EXPECT_NE(back, nullptr);
}

// One quad intersects a pent with an occluded side.
TEST(DrawPolygonSplitTest, SlimClip) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(9.0f, 9.0f, 5.000f));
  vertices_b.push_back(gfx::Point3F(1.0f, 1.0f, 0.001f));
  vertices_b.push_back(gfx::Point3F(1.0f, 1.0f, 0.000f));
  vertices_b.push_back(gfx::Point3F(1.002f, 1.002f, -0.005f));
  vertices_b.push_back(gfx::Point3F(9.0f, 9.0f, -4.000f));

  CREATE_NEW_DRAW_POLYGON_PTR(polygon_a, vertices_a,
                              gfx::Vector3dF(0.0f, 0.0f, 1.0f), 0);
  CREATE_NEW_DRAW_POLYGON_PTR(
      polygon_b, vertices_b,
      gfx::Vector3dF(sqrt(2) / 2, -sqrt(2) / 2, 0.000000), 1);

  // These are well formed, convex polygons.
  EXPECT_TRUE(IsPlanarForTesting(*polygon_a));
  EXPECT_TRUE(IsConvexForTesting(*polygon_a));
  EXPECT_TRUE(IsPlanarForTesting(*polygon_b));
  EXPECT_TRUE(IsConvexForTesting(*polygon_b));

  std::unique_ptr<DrawPolygon> front_polygon;
  std::unique_ptr<DrawPolygon> back_polygon;
  bool is_coplanar;

  polygon_a->SplitPolygon(std::move(polygon_b), &front_polygon, &back_polygon,
                          &is_coplanar);

  EXPECT_FALSE(is_coplanar);
  EXPECT_TRUE(front_polygon != nullptr);
  EXPECT_TRUE(back_polygon != nullptr);
}

// One quad intersects another and becomes two pieces.
TEST(DrawPolygonSplitTest, BasicSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, -5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, -5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, 5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 5.0f));

  CREATE_NEW_DRAW_POLYGON_PTR(polygon_a, vertices_a,
                              gfx::Vector3dF(0.0f, 0.0f, 1.0f), 0);
  CREATE_NEW_DRAW_POLYGON_PTR(polygon_b, vertices_b,
                              gfx::Vector3dF(-1.0f, 0.0f, 0.0f), 1);

  std::unique_ptr<DrawPolygon> front_polygon;
  std::unique_ptr<DrawPolygon> back_polygon;
  bool is_coplanar;

  polygon_a->SplitPolygon(std::move(polygon_b), &front_polygon, &back_polygon,
                          &is_coplanar);
  EXPECT_FALSE(is_coplanar);
  EXPECT_TRUE(front_polygon != nullptr);
  EXPECT_TRUE(back_polygon != nullptr);

  std::vector<gfx::Point3F> test_points_a;
  test_points_a.push_back(gfx::Point3F(5.0f, 0.0f, 0.0f));
  test_points_a.push_back(gfx::Point3F(5.0f, 0.0f, 5.0f));
  test_points_a.push_back(gfx::Point3F(5.0f, 10.0f, 5.0f));
  test_points_a.push_back(gfx::Point3F(5.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> test_points_b;
  test_points_b.push_back(gfx::Point3F(5.0f, 10.0f, 0.0f));
  test_points_b.push_back(gfx::Point3F(5.0f, 10.0f, -5.0f));
  test_points_b.push_back(gfx::Point3F(5.0f, 0.0f, -5.0f));
  test_points_b.push_back(gfx::Point3F(5.0f, 0.0f, 0.0f));
  ValidatePoints(*front_polygon, test_points_a);
  ValidatePoints(*back_polygon, test_points_b);

  EXPECT_EQ(4u, front_polygon->points().size());
  EXPECT_EQ(4u, back_polygon->points().size());
}

// In this test we cut the corner of a quad so that it creates a triangle and
// a pentagon as a result.
TEST(DrawPolygonSplitTest, AngledSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 10.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 10.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(2.0f, 5.0f, 1.0f));
  vertices_b.push_back(gfx::Point3F(2.0f, -5.0f, 1.0f));
  vertices_b.push_back(gfx::Point3F(-1.0f, -5.0f, -2.0f));
  vertices_b.push_back(gfx::Point3F(-1.0f, 5.0f, -2.0f));

  CREATE_NEW_DRAW_POLYGON_PTR(polygon_a, vertices_a,
                              gfx::Vector3dF(0.0f, 1.0f, 0.0f), 0);
  CREATE_NEW_DRAW_POLYGON_PTR(polygon_b, vertices_b,
                              gfx::Vector3dF(0.707107f, 0.0f, -0.707107f), 1);

  std::unique_ptr<DrawPolygon> front_polygon;
  std::unique_ptr<DrawPolygon> back_polygon;
  bool is_coplanar;

  polygon_b->SplitPolygon(std::move(polygon_a), &front_polygon, &back_polygon,
                          &is_coplanar);
  EXPECT_FALSE(is_coplanar);
  EXPECT_TRUE(front_polygon != nullptr);
  EXPECT_TRUE(back_polygon != nullptr);

  std::vector<gfx::Point3F> test_points_a;
  test_points_a.push_back(gfx::Point3F(10.0f, 0.0f, 9.0f));
  test_points_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  test_points_a.push_back(gfx::Point3F(1.0f, 0.0f, 0.0f));
  std::vector<gfx::Point3F> test_points_b;
  test_points_b.push_back(gfx::Point3F(1.0f, 0.0f, 0.0f));
  test_points_b.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  test_points_b.push_back(gfx::Point3F(0.0f, 0.0f, 10.0f));
  test_points_b.push_back(gfx::Point3F(10.0f, 0.0f, 10.0f));
  test_points_b.push_back(gfx::Point3F(10.0f, 0.0f, 9.0f));

  ValidatePointsWithinDeltaOf(*front_polygon, test_points_a, 1e-6f);
  ValidatePointsWithinDeltaOf(*back_polygon, test_points_b, 1e-6f);
}

// This test was derived from crbug.com/693826. An almost coplanar
// pair of polygons are used for splitting. In this case, the
// splitting plane distance signs are [ 0 0 + - ]. This configuration
// represents a case where snapping to the splitting plane causes the
// polygon to become twisted. Splitting should still give a valid
// result, indicated by all four of the input split polygon vertices
// being present in the output polygons.
TEST(DrawPolygonSplitTest, AlmostCoplanarSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(723.814758300781250f, 552.810119628906250f,
                                    -206.656036376953125f));
  vertices_a.push_back(gfx::Point3F(797.634155273437500f, 549.095703125000000f,
                                    -209.802902221679688f));
  vertices_a.push_back(gfx::Point3F(799.264648437500000f, 490.325805664062500f,
                                    -172.261627197265625f));
  vertices_a.push_back(gfx::Point3F(720.732421875000000f, 493.944458007812500f,
                                    -168.700469970703125f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(720.631286621093750f, 487.595977783203125f,
                                    -164.681198120117188f));
  vertices_b.push_back(gfx::Point3F(799.672851562500000f, 484.059020996093750f,
                                    -168.219161987304688f));
  vertices_b.push_back(gfx::Point3F(801.565490722656250f, 416.416809082031250f,
                                    -125.007690429687500f));
  vertices_b.push_back(gfx::Point3F(717.096801757812500f, 419.792327880859375f,
                                    -120.967689514160156f));

  CREATE_NEW_DRAW_POLYGON_PTR(
      splitting_polygon, vertices_a,
      gfx::Vector3dF(-0.062916249036789f, -0.538499474525452f,
                     -0.840273618698120f),
      0);
  CREATE_NEW_DRAW_POLYGON_PTR(
      split_polygon, vertices_b,
      gfx::Vector3dF(-0.061713f, -0.538550f, -0.840330f), 1);

  std::unique_ptr<DrawPolygon> front_polygon;
  std::unique_ptr<DrawPolygon> back_polygon;
  bool is_coplanar;

  splitting_polygon->SplitPolygon(std::move(split_polygon), &front_polygon,
                                  &back_polygon, &is_coplanar);

  EXPECT_FALSE(is_coplanar);
  EXPECT_TRUE(front_polygon != nullptr);
  EXPECT_TRUE(back_polygon != nullptr);

  for (auto vertex : vertices_b) {
    EXPECT_TRUE(base::Contains(front_polygon->points(), vertex) ||
                base::Contains(back_polygon->points(), vertex));
  }
}

// In this test we cut the corner of a quad so that it creates a triangle and
// a pentagon as a result, and then cut the pentagon.
TEST(DrawPolygonSplitTest, DoubleSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 10.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 10.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(2.0f, 5.0f, 1.0f));
  vertices_b.push_back(gfx::Point3F(2.0f, -5.0f, 1.0f));
  vertices_b.push_back(gfx::Point3F(-1.0f, -5.0f, -2.0f));
  vertices_b.push_back(gfx::Point3F(-1.0f, 5.0f, -2.0f));

  CREATE_NEW_DRAW_POLYGON_PTR(polygon_a, vertices_a,
                              gfx::Vector3dF(0.0f, 1.0f, 0.0f), 0);
  CREATE_NEW_DRAW_POLYGON_PTR(polygon_b, vertices_b,
                              gfx::Vector3dF(sqrt(2) / 2, 0.0f, -sqrt(2) / 2),
                              1);

  std::unique_ptr<DrawPolygon> front_polygon;
  std::unique_ptr<DrawPolygon> back_polygon;
  bool is_coplanar;

  polygon_b->SplitPolygon(std::move(polygon_a), &front_polygon, &back_polygon,
                          &is_coplanar);
  EXPECT_FALSE(is_coplanar);
  EXPECT_TRUE(front_polygon != nullptr);
  EXPECT_TRUE(back_polygon != nullptr);

  EXPECT_EQ(3u, front_polygon->points().size());
  EXPECT_EQ(5u, back_polygon->points().size());
  std::vector<gfx::Point3F> saved_back_polygon_vertices =
      back_polygon->points();

  std::vector<gfx::Point3F> vertices_c;
  vertices_c.push_back(gfx::Point3F(0.0f, 0.0f, 10.0f));
  vertices_c.push_back(gfx::Point3F(1.0f, -0.05f, 0.0f));
  vertices_c.push_back(gfx::Point3F(10.0f, 0.05f, 9.0f));

  CREATE_NEW_DRAW_POLYGON_PTR(polygon_c, vertices_c,
                              gfx::Vector3dF(0.005555f, -0.99997f, 0.005555f),
                              0);
  polygon_c->RecomputeNormalForTesting();

  std::unique_ptr<DrawPolygon> second_front_polygon;
  std::unique_ptr<DrawPolygon> second_back_polygon;

  polygon_c->SplitPolygon(std::move(back_polygon), &second_front_polygon,
                          &second_back_polygon, &is_coplanar);
  EXPECT_FALSE(is_coplanar);
  EXPECT_TRUE(second_front_polygon != nullptr);
  EXPECT_TRUE(second_back_polygon != nullptr);

  EXPECT_EQ(4u, second_front_polygon->points().size());
  EXPECT_EQ(3u, second_back_polygon->points().size());

  for (auto vertex : saved_back_polygon_vertices) {
    EXPECT_TRUE(base::Contains(second_front_polygon->points(), vertex) ||
                base::Contains(second_back_polygon->points(), vertex));
  }
}

TEST(DrawPolygonTransformTest, TransformNormal) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(1.0f, 0.0f, 1.0f));
  vertices_a.push_back(gfx::Point3F(-1.0f, 0.0f, -1.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 1.0f, 0.0f));
  CREATE_NEW_DRAW_POLYGON(polygon_a, vertices_a,
                          gfx::Vector3dF(sqrt(2) / 2, 0.0f, -sqrt(2) / 2), 0);
  EXPECT_NORMAL(polygon_a, sqrt(2) / 2, 0.0f, -sqrt(2) / 2);

  gfx::Transform transform;
  transform.RotateAboutYAxis(45.0f);
  // This would transform the vertices as well, but we are transforming a
  // DrawPolygon with 0 vertices just to make sure our normal transformation
  // using the inverse tranpose matrix gives us the right result.
  polygon_a.TransformToScreenSpace(transform);

  // Note: We use EXPECT_FLOAT_WITHIN_EPSILON instead of EXPECT_FLOAT_EQUAL here
  // because some architectures (e.g., Arm64) employ a fused multiply-add
  // instruction which causes rounding asymmetry and reduces precision.
  // http://crbug.com/401117.
  EXPECT_NORMAL(polygon_a, 0.0f, 0.0f, -1.0f);
}

}  // namespace
}  // namespace viz
