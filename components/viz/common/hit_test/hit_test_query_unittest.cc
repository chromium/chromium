// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/hit_test/hit_test_query.h"

#include <cstdint>

#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace test {

class HitTestQueryTest : public testing::Test {
 public:
  HitTestQueryTest() = default;

  HitTestQueryTest(const HitTestQueryTest&) = delete;
  HitTestQueryTest& operator=(const HitTestQueryTest&) = delete;

  ~HitTestQueryTest() override = default;

  void SendHitTestData() {
    hit_test_query_.OnAggregatedHitTestRegionListUpdated(active_data_);
  }

 protected:
  HitTestQuery& hit_test_query() { return hit_test_query_; }

  std::vector<AggregatedHitTestRegion> active_data_;

 private:
  HitTestQuery hit_test_query_{std::nullopt};
};

// One surface.
//
//  +e---------+
//  |          |
//  |          |
//  |          |
//  +----------+
//
TEST_F(HitTestQueryTest, OneSurface) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  gfx::Rect e_bounds = gfx::Rect(0, 0, 600, 600);
  gfx::Transform transform_e_to_e;
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds, transform_e_to_e, 0));  // e
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(600, 600);
  gfx::PointF point3(0, 0);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  // point2 is on the bounds of e so no target found.
  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, FrameSinkId());
  EXPECT_EQ(target2.location_in_target, gfx::PointF());
  EXPECT_FALSE(target2.flags);

  // There's a valid Target for point3, see Rect::Contains.
  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, e_id);
  EXPECT_EQ(target3.location_in_target, point3);
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

// One embedder with two children.
//
//  +e------------+     Point   maps to
//  | +c1-+ +c2---|     -----   -------
//  |1|   | |     |      1        e
//  | | 2 | | 3   | 4    2        c1
//  | +---+ |     |      3        c2
//  +-------------+      4        none
//
TEST_F(HitTestQueryTest, OneEmbedderTwoChildren) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId c2_id = FrameSinkId(3, 3);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 200, 200);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 400, 400);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_e_to_c2;
  transform_e_to_c1.Translate(-100, -100);
  transform_e_to_c2.Translate(-300, -300);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 2));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c1_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c1_bounds_in_e, transform_e_to_c1, 0));  // c1
  active_data_.push_back(AggregatedHitTestRegion(
      c2_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c2_bounds_in_e, transform_e_to_c2, 0));  // c2
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(99, 200);
  gfx::PointF point2(150, 150);
  gfx::PointF point3(400, 400);
  gfx::PointF point4(650, 350);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, c1_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(50, 50));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, c2_id);
  EXPECT_EQ(target3.location_in_target, gfx::PointF(100, 100));
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point4);
  EXPECT_EQ(target4.frame_sink_id, FrameSinkId());
  EXPECT_EQ(target4.location_in_target, gfx::PointF());
  EXPECT_FALSE(target4.flags);
}

// One embedder with a rotated child.
TEST_F(HitTestQueryTest, OneEmbedderRotatedChild) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 1000, 1000);
  gfx::Transform transform_e_to_e, transform_e_to_c;
  transform_e_to_c.Translate(-100, -100);
  transform_e_to_c.Skew(2, 3);
  transform_e_to_c.Scale(.5f, .7f);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 1));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c_bounds_in_e, transform_e_to_c, 0));  // c
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(150, 120);  // Point(-22.07, -12.07) after transform.
  gfx::PointF point2(550, 400);  // Point(184.78, 194.41) after transform.

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, c_id);
  EXPECT_NEAR(target2.location_in_target.x(), 185, .5);
  EXPECT_NEAR(target2.location_in_target.y(), 194, .5);
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

// One embedder with a clipped child with a tab and transparent background.
//
//  +e-------------+
//  |   +c---------|     Point   maps to
//  | 1 |+a--+     |     -----   -------
//  |   || 2 |  3  |       1        e
//  |   |+b--------|       2        a
//  |   ||         |       3        e ( transparent area in c )
//  |   ||   4     |       4        b
//  +--------------+
//
TEST_F(HitTestQueryTest, ClippedChildWithTabAndTransparentBackground) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect a_bounds_in_c = gfx::Rect(0, 0, 200, 100);
  gfx::Rect b_bounds_in_c = gfx::Rect(0, 0, 800, 600);
  gfx::Transform transform_e_to_e, transform_e_to_c, transform_c_to_a,
      transform_c_to_b;
  transform_e_to_c.Translate(-200, -100);
  transform_c_to_b.Translate(0, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 3));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c_id, HitTestRegionFlags::kHitTestChildSurface,
                              c_bounds_in_e, transform_e_to_c, 2));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c, transform_c_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0));  // b
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);
  gfx::PointF point3(403, 103);
  gfx::PointF point4(202, 202);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, a_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, e_id);
  EXPECT_EQ(target3.location_in_target, point3);
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point4);
  EXPECT_EQ(target4.frame_sink_id, b_id);
  EXPECT_EQ(target4.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target4.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

// One embedder with a clipped child with a tab and transparent background, and
// a child d under that.
//
//  +e-------------+
//  |      +d------|
//  |   +c-|-------|     Point   maps to
//  | 1 |+a|-+     |     -----   -------
//  |   || 2 |  3  |       1        e
//  |   |+b|-------|       2        a
//  |   || |       |       3        d
//  |   || | 4     |       4        b
//  +--------------+
//
TEST_F(HitTestQueryTest, ClippedChildWithChildUnderneath) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  FrameSinkId d_id = FrameSinkId(5, 5);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect a_bounds_in_c = gfx::Rect(0, 0, 200, 100);
  gfx::Rect b_bounds_in_c = gfx::Rect(0, 0, 800, 600);
  gfx::Rect d_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c, transform_c_to_a,
      transform_c_to_b, transform_e_to_d;
  transform_e_to_c.Translate(-200, -100);
  transform_c_to_b.Translate(0, -100);
  transform_e_to_d.Translate(-400, -50);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 4));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c_id, HitTestRegionFlags::kHitTestChildSurface,
                              c_bounds_in_e, transform_e_to_c, 2));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c, transform_c_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0));  // b
  active_data_.push_back(AggregatedHitTestRegion(
      d_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      d_bounds_in_e, transform_e_to_d, 0));  // d
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);
  gfx::PointF point3(450, 150);
  gfx::PointF point4(202, 202);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, a_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, d_id);
  EXPECT_EQ(target3.location_in_target, gfx::PointF(50, 100));
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point4);
  EXPECT_EQ(target4.frame_sink_id, b_id);
  EXPECT_EQ(target4.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target4.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

// Tests transforming location to be in target's coordinate system given the
// target's ancestor list, in the case of ClippedChildWithChildUnderneath test.
TEST_F(HitTestQueryTest, ClippedChildWithChildUnderneathTransform) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  FrameSinkId d_id = FrameSinkId(5, 5);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect a_bounds_in_c = gfx::Rect(0, 0, 200, 100);
  gfx::Rect b_bounds_in_c = gfx::Rect(0, 0, 800, 600);
  gfx::Rect d_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c, transform_c_to_a,
      transform_c_to_b, transform_e_to_d;
  transform_e_to_c.Translate(-200, -100);
  transform_e_to_d.Translate(-400, -50);
  transform_c_to_b.Translate(0, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 4));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c_id, HitTestRegionFlags::kHitTestChildSurface,
                              c_bounds_in_e, transform_e_to_c, 2));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c, transform_c_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0));  // b
  active_data_.push_back(AggregatedHitTestRegion(
      d_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      d_bounds_in_e, transform_e_to_d, 0));  // d
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);
  gfx::PointF point3(450, 150);
  gfx::PointF point4(202, 202);

  std::vector<FrameSinkId> target_ancestors1{e_id};
  gfx::PointF transformed_point;
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors1, point1, &transformed_point));
  EXPECT_EQ(transformed_point, point1);
  std::vector<FrameSinkId> target_ancestors2{a_id, c_id, e_id};
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors2, point2, &transformed_point));
  EXPECT_EQ(transformed_point, gfx::PointF(2, 2));
  std::vector<FrameSinkId> target_ancestors3{d_id, e_id};
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors3, point3, &transformed_point));
  EXPECT_EQ(transformed_point, gfx::PointF(50, 100));
  std::vector<FrameSinkId> target_ancestors4{b_id, c_id, e_id};
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors4, point4, &transformed_point));
  EXPECT_EQ(transformed_point, gfx::PointF(2, 2));
}

// One embedder with two clipped children with a tab and transparent background.
//
//  +e-------------+
//  |   +c1--------|     Point   maps to
//  | 1 |+a--+     |     -----   -------
//  |   || 2 |  3  |       1        e
//  |   |+b--------|       2        a
//  |   ||         |       3        e ( transparent area in c1 )
//  |   ||   4     |       4        b
//  |   +----------|       5        g
//  |   +c2--------|       6        e ( transparent area in c2 )
//  |   |+g--+     |       7        h
//  |   || 5 |  6  |
//  |   |+h--------|
//  |   ||         |
//  |   ||   7     |
//  +--------------+
//
TEST_F(HitTestQueryTest, ClippedChildrenWithTabAndTransparentBackground) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  FrameSinkId c2_id = FrameSinkId(5, 5);
  FrameSinkId g_id = FrameSinkId(6, 6);
  FrameSinkId h_id = FrameSinkId(7, 7);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 1200);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 800, 500);
  gfx::Rect a_bounds_in_c1 = gfx::Rect(0, 0, 200, 100);
  gfx::Rect b_bounds_in_c1 = gfx::Rect(0, 0, 800, 400);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 800, 500);
  gfx::Rect g_bounds_in_c2 = gfx::Rect(0, 0, 200, 100);
  gfx::Rect h_bounds_in_c2 = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_c1_to_a,
      transform_c1_to_b, transform_e_to_c2, transform_c2_to_g,
      transform_c2_to_h;
  transform_e_to_c1.Translate(-200, -100);
  transform_c1_to_b.Translate(0, -100);
  transform_e_to_c2.Translate(-200, -700);
  transform_c2_to_h.Translate(0, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 6));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c1_id, HitTestRegionFlags::kHitTestChildSurface,
                              c1_bounds_in_e, transform_e_to_c1, 2));  // c1
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c1, transform_c1_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c1, transform_c1_to_b, 0));  // b
  active_data_.push_back(
      AggregatedHitTestRegion(c2_id, HitTestRegionFlags::kHitTestChildSurface,
                              c2_bounds_in_e, transform_e_to_c2, 2));  // c2
  active_data_.push_back(AggregatedHitTestRegion(
      g_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      g_bounds_in_c2, transform_c2_to_g, 0));  // g
  active_data_.push_back(AggregatedHitTestRegion(
      h_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      h_bounds_in_c2, transform_c2_to_h, 0));  // h
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);
  gfx::PointF point3(403, 103);
  gfx::PointF point4(202, 202);
  gfx::PointF point5(250, 750);
  gfx::PointF point6(450, 750);
  gfx::PointF point7(350, 1100);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, a_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, e_id);
  EXPECT_EQ(target3.location_in_target, point3);
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point4);
  EXPECT_EQ(target4.frame_sink_id, b_id);
  EXPECT_EQ(target4.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target4.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target5 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point5);
  EXPECT_EQ(target5.frame_sink_id, g_id);
  EXPECT_EQ(target5.location_in_target, gfx::PointF(50, 50));
  EXPECT_EQ(target5.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target6 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point6);
  EXPECT_EQ(target6.frame_sink_id, e_id);
  EXPECT_EQ(target6.location_in_target, point6);
  EXPECT_EQ(target6.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target7 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point7);
  EXPECT_EQ(target7.frame_sink_id, h_id);
  EXPECT_EQ(target7.location_in_target, gfx::PointF(150, 300));
  EXPECT_EQ(target7.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

// Tests transforming location to be in target's coordinate system given the
// target's ancestor list, in the case of
// ClippedChildrenWithTabAndTransparentBackground test.
TEST_F(HitTestQueryTest,
       ClippedChildrenWithTabAndTransparentBackgroundTransform) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  FrameSinkId c2_id = FrameSinkId(5, 5);
  FrameSinkId g_id = FrameSinkId(6, 6);
  FrameSinkId h_id = FrameSinkId(7, 7);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 1200);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 800, 500);
  gfx::Rect a_bounds_in_c1 = gfx::Rect(0, 0, 200, 100);
  gfx::Rect b_bounds_in_c1 = gfx::Rect(0, 0, 800, 400);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 800, 500);
  gfx::Rect g_bounds_in_c2 = gfx::Rect(0, 0, 200, 100);
  gfx::Rect h_bounds_in_c2 = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_c1_to_a,
      transform_c1_to_b, transform_e_to_c2, transform_c2_to_g,
      transform_c2_to_h;
  transform_e_to_c1.Translate(-200, -100);
  transform_c1_to_b.Translate(0, -100);
  transform_e_to_c2.Translate(-200, -700);
  transform_c2_to_h.Translate(0, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 6));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c1_id, HitTestRegionFlags::kHitTestChildSurface,
                              c1_bounds_in_e, transform_e_to_c1, 2));  // c1
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c1, transform_c1_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c1, transform_c1_to_b, 0));  // b
  active_data_.push_back(
      AggregatedHitTestRegion(c2_id, HitTestRegionFlags::kHitTestChildSurface,
                              c2_bounds_in_e, transform_e_to_c2, 2));  // c2
  active_data_.push_back(AggregatedHitTestRegion(
      g_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      g_bounds_in_c2, transform_c2_to_g, 0));  // g
  active_data_.push_back(AggregatedHitTestRegion(
      h_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      h_bounds_in_c2, transform_c2_to_h, 0));  // h
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);
  gfx::PointF point3(403, 103);
  gfx::PointF point4(202, 202);
  gfx::PointF point5(250, 750);
  gfx::PointF point6(450, 750);
  gfx::PointF point7(350, 1100);

  std::vector<FrameSinkId> target_ancestors1{e_id};
  gfx::PointF transformed_point;
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors1, point1, &transformed_point));
  EXPECT_EQ(transformed_point, point1);
  std::vector<FrameSinkId> target_ancestors2{a_id, c1_id, e_id};
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors2, point2, &transformed_point));
  EXPECT_EQ(transformed_point, gfx::PointF(2, 2));
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors1, point3, &transformed_point));
  EXPECT_EQ(transformed_point, point3);
  std::vector<FrameSinkId> target_ancestors3{b_id, c1_id, e_id};
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors3, point4, &transformed_point));
  EXPECT_EQ(transformed_point, gfx::PointF(2, 2));
  std::vector<FrameSinkId> target_ancestors4{g_id, c2_id, e_id};
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors4, point5, &transformed_point));
  EXPECT_EQ(transformed_point, gfx::PointF(50, 50));
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors1, point6, &transformed_point));
  EXPECT_EQ(transformed_point, point6);
  std::vector<FrameSinkId> target_ancestors5{h_id, c2_id, e_id};
  EXPECT_TRUE(hit_test_query().TransformLocationForTarget(
      target_ancestors5, point7, &transformed_point));
  EXPECT_EQ(transformed_point, gfx::PointF(150, 300));
}

// Children that are multiple layers deep.
//
//  +e--------------------+
//  |          +c2--------|     Point   maps to
//  | +c1------|----+     |     -----   -------
//  |1| +a-----|---+|     |       1        e
//  | | |+b----|--+||     |       2        g
//  | | ||+g--+|  |||     |       3        b
//  | | ||| 2 || 3|||  4  |       4        c2
//  | | ||+---+|  |||     |
//  | | |+-----|--+||     |
//  | | +------| --+|     |
//  | +--------|----+     |
//  +---------------------+
//
TEST_F(HitTestQueryTest, MultipleLayerChild) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  FrameSinkId g_id = FrameSinkId(5, 5);
  FrameSinkId c2_id = FrameSinkId(6, 6);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 1000, 1000);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect a_bounds_in_c1 = gfx::Rect(0, 0, 700, 700);
  gfx::Rect b_bounds_in_a = gfx::Rect(0, 0, 600, 600);
  gfx::Rect g_bounds_in_b = gfx::Rect(0, 0, 200, 200);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_c1_to_a,
      transform_a_to_b, transform_b_to_g, transform_e_to_c2;
  transform_e_to_c1.Translate(-100, -100);
  transform_a_to_b.Translate(-50, -30);
  transform_b_to_g.Translate(-150, -200);
  transform_e_to_c2.Translate(-400, -50);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 5));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c1_id, HitTestRegionFlags::kHitTestChildSurface,
                              c1_bounds_in_e, transform_e_to_c1, 3));  // c1
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c1, transform_c1_to_a, 2));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_a, transform_a_to_b, 1));  // b
  active_data_.push_back(AggregatedHitTestRegion(
      g_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      g_bounds_in_b, transform_b_to_g, 0));  // g
  active_data_.push_back(AggregatedHitTestRegion(
      c2_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c2_bounds_in_e, transform_e_to_c2, 0));  // c2
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(300, 350);
  gfx::PointF point3(550, 350);
  gfx::PointF point4(900, 350);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, g_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(0, 20));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, b_id);
  EXPECT_EQ(target3.location_in_target, gfx::PointF(400, 220));
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point4);
  EXPECT_EQ(target4.frame_sink_id, c2_id);
  EXPECT_EQ(target4.location_in_target, gfx::PointF(500, 300));
  EXPECT_EQ(target4.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

// Multiple layers deep of transparent children.
//
//  +e--------------------+
//  |          +c2--------|     Point   maps to
//  | +c1------|----+     |     -----   -------
//  |1| +a-----|---+|     |       1        e
//  | | |+b----|--+||     |       2        e
//  | | ||+g--+|  |||     |       3        c2
//  | | ||| 2 || 3|||  4  |       4        c2
//  | | ||+---+|  |||     |
//  | | |+-----|--+||     |
//  | | +------| --+|     |
//  | +--------|----+     |
//  +---------------------+
//
TEST_F(HitTestQueryTest, MultipleLayerTransparentChild) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  FrameSinkId g_id = FrameSinkId(5, 5);
  FrameSinkId c2_id = FrameSinkId(6, 6);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 1000, 1000);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect a_bounds_in_c1 = gfx::Rect(0, 0, 700, 700);
  gfx::Rect b_bounds_in_a = gfx::Rect(0, 0, 600, 600);
  gfx::Rect g_bounds_in_b = gfx::Rect(0, 0, 200, 200);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_c1_to_a,
      transform_a_to_b, transform_b_to_g, transform_e_to_c2;
  transform_e_to_c1.Translate(-100, -100);
  transform_a_to_b.Translate(-50, -30);
  transform_b_to_g.Translate(-150, -200);
  transform_e_to_c2.Translate(-400, -50);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 5));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c1_id, HitTestRegionFlags::kHitTestChildSurface,
                              c1_bounds_in_e, transform_e_to_c1, 3));  // c1
  active_data_.push_back(
      AggregatedHitTestRegion(a_id, HitTestRegionFlags::kHitTestChildSurface,
                              a_bounds_in_c1, transform_c1_to_a, 2));  // a
  active_data_.push_back(
      AggregatedHitTestRegion(b_id, HitTestRegionFlags::kHitTestChildSurface,
                              b_bounds_in_a, transform_a_to_b, 1));  // b
  active_data_.push_back(
      AggregatedHitTestRegion(g_id, HitTestRegionFlags::kHitTestChildSurface,
                              g_bounds_in_b, transform_b_to_g, 0));  // g
  active_data_.push_back(AggregatedHitTestRegion(
      c2_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c2_bounds_in_e, transform_e_to_c2, 0));  // c2
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(300, 350);
  gfx::PointF point3(450, 350);
  gfx::PointF point4(900, 350);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_TRUE(target1.flags);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, e_id);
  EXPECT_EQ(target2.location_in_target, point2);
  EXPECT_TRUE(target2.flags);

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, c2_id);
  EXPECT_EQ(target3.location_in_target, gfx::PointF(50, 300));
  EXPECT_TRUE(target3.flags);

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point4);
  EXPECT_EQ(target4.frame_sink_id, c2_id);
  EXPECT_EQ(target4.location_in_target, gfx::PointF(500, 300));
  EXPECT_TRUE(target4.flags);
}

TEST_F(HitTestQueryTest, InvalidAggregatedHitTestRegionData) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect a_bounds_in_c = gfx::Rect(0, 0, 200, 100);
  gfx::Rect b_bounds_in_c = gfx::Rect(0, 0, 800, 600);
  gfx::Transform transform_e_to_e, transform_e_to_c, transform_c_to_a,
      transform_c_to_b;
  transform_e_to_c.Translate(-200, -100);
  transform_c_to_b.Translate(0, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 3));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c_id, HitTestRegionFlags::kHitTestChildSurface, c_bounds_in_e,
      transform_e_to_c, INT32_MIN));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c, transform_c_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0));  // b
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);

  // |child_count| is invalid, which is a security fault. For now, check to see
  // if the returned Target is invalid.
  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, FrameSinkId());
  EXPECT_EQ(target1.location_in_target, gfx::PointF());
  EXPECT_FALSE(target1.flags);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, FrameSinkId());
  EXPECT_EQ(target2.location_in_target, gfx::PointF());
  EXPECT_FALSE(target2.flags);

  active_data_.clear();
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, INT32_MAX));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c_id, HitTestRegionFlags::kHitTestChildSurface,
                              c_bounds_in_e, transform_e_to_c, 2));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c, transform_c_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0));  // b
  SendHitTestData();

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target3.frame_sink_id, FrameSinkId());
  EXPECT_EQ(target3.location_in_target, gfx::PointF());
  EXPECT_FALSE(target3.flags);

  active_data_.clear();
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 3));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c_id, HitTestRegionFlags::kHitTestChildSurface,
                              c_bounds_in_e, transform_e_to_c, 3));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c, transform_c_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0));  // b
  SendHitTestData();

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target4.frame_sink_id, FrameSinkId());
  EXPECT_EQ(target4.location_in_target, gfx::PointF());
  EXPECT_FALSE(target4.flags);
}

// Tests flags kHitTestMouse and kHitTestTouch.
TEST_F(HitTestQueryTest, MouseTouchFlags) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId c2_id = FrameSinkId(3, 3);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 300, 200);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 350, 250);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_e_to_c2;
  transform_e_to_c1.Translate(-100, -100);
  transform_e_to_c2.Translate(-75, -75);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse |
          HitTestRegionFlags::kHitTestTouch,
      e_bounds_in_e, transform_e_to_e, 2));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c1_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c1_bounds_in_e, transform_e_to_c1, 0));  // c1
  active_data_.push_back(AggregatedHitTestRegion(
      c2_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestTouch,
      c2_bounds_in_e, transform_e_to_c2, 0));  // c2
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(80, 80);
  gfx::PointF point2(150, 150);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse |
                               HitTestRegionFlags::kHitTestTouch);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::TOUCH, point1);
  EXPECT_EQ(target2.frame_sink_id, c2_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(5, 5));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestTouch);

  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target3.frame_sink_id, c1_id);
  EXPECT_EQ(target3.location_in_target, gfx::PointF(50, 50));
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target4 =
      hit_test_query().FindTargetForLocation(EventSource::TOUCH, point2);
  EXPECT_EQ(target4.frame_sink_id, c2_id);
  EXPECT_EQ(target4.location_in_target, gfx::PointF(75, 75));
  EXPECT_EQ(target4.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestTouch);
}

TEST_F(HitTestQueryTest, RootHitTestAskFlag) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  gfx::Rect e_bounds = gfx::Rect(0, 0, 600, 600);
  gfx::Transform transform_e_to_e;
  active_data_.push_back(AggregatedHitTestRegion(
      e_id, HitTestRegionFlags::kHitTestAsk | HitTestRegionFlags::kHitTestMouse,
      e_bounds, transform_e_to_e, 0,
      AsyncHitTestReasons::kUseDrawQuadData));  // e
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(600, 600);

  // point1 is inside e but we have to ask clients for targeting.
  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestAsk |
                               HitTestRegionFlags::kHitTestMouse);

  // point2 is on the bounds of e so no target found.
  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, FrameSinkId());
  EXPECT_EQ(target2.location_in_target, gfx::PointF());
  EXPECT_FALSE(target2.flags);
}

// One embedder with two children.
//
//  +e------------+     Point   maps to
//  | +c1-+ +c2---|     -----   -------
//  |1|   | |     |      1        e
//  | | 2 | | 3   |      2        c1
//  | +---+ |     |      3        c2
//  +-------------+
//
TEST_F(HitTestQueryTest, ChildHitTestAskFlag) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId c2_id = FrameSinkId(3, 3);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 200, 200);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 400, 400);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_e_to_c2;
  transform_e_to_c1.Translate(-100, -100);
  transform_e_to_c2.Translate(-300, -300);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 2));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c1_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c1_bounds_in_e, transform_e_to_c1, 0));  // c1
  active_data_.push_back(AggregatedHitTestRegion(
      c2_id,
      HitTestRegionFlags::kHitTestAsk | HitTestRegionFlags::kHitTestMouse,
      c2_bounds_in_e, transform_e_to_c2, 0,
      AsyncHitTestReasons::kOverlappedRegion));  // c2
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(99, 200);
  gfx::PointF point2(150, 150);
  gfx::PointF point3(400, 400);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, c1_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(50, 50));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  // point3 is inside c2 but we have to ask clients for targeting. Event
  // shouldn't go back to e.
  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, c2_id);
  EXPECT_EQ(target3.location_in_target, gfx::PointF(100, 100));
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestAsk |
                               HitTestRegionFlags::kHitTestMouse);
}

// One embedder with nested OOPIFs. When the mid-level iframe has kHitTestAsk
// flag we should do async hit test and skip checking its descendants.
//
//  +e-------------+
//  |   +c---------|     Point   maps to
//  | 1 |    2     |     -----   -------
//  |   |          |       1        e
//  |   |+b--------|       2        c
//  |   ||         |       3        c
//  |   ||   3     |
//  +--------------+
//
TEST_F(HitTestQueryTest, NestedOOPIFs) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  FrameSinkId b_id = FrameSinkId(3, 3);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect b_bounds_in_c = gfx::Rect(0, 0, 800, 600);
  gfx::Transform transform_e_to_e, transform_e_to_c, transform_c_to_b;
  transform_e_to_c.Translate(-200, -100);
  transform_c_to_b.Translate(0, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 2));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c_id,
      HitTestRegionFlags::kHitTestChildSurface |
          HitTestRegionFlags::kHitTestAsk | HitTestRegionFlags::kHitTestMouse,
      c_bounds_in_e, transform_e_to_c, 1,
      AsyncHitTestReasons::kOverlappedRegion));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      b_id, HitTestRegionFlags::kHitTestAsk | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0,
      AsyncHitTestReasons::kOverlappedRegion));  // b
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);
  gfx::PointF point3(202, 202);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  // c is the deepest OOPIF for point2, return c with ask flag.
  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, c_id);
  // point2 + transform_e_to_c  = (2, 2).
  EXPECT_EQ(target2.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestChildSurface |
                               HitTestRegionFlags::kHitTestAsk |
                               HitTestRegionFlags::kHitTestMouse);

  // b is the deepest OOPIF for point3, but c has the ask flag. Return c
  // accordingly.
  Target target3 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point3);
  EXPECT_EQ(target3.frame_sink_id, c_id);
  // point3 + transform_e_to_c  = (2, 102).
  EXPECT_EQ(target3.location_in_target, gfx::PointF(2, 102));
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestChildSurface |
                               HitTestRegionFlags::kHitTestAsk |
                               HitTestRegionFlags::kHitTestMouse);
}

// Tests getting the transform from root to a given target.
TEST_F(HitTestQueryTest, GetTransformToTarget) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  FrameSinkId a_id = FrameSinkId(3, 3);
  FrameSinkId b_id = FrameSinkId(4, 4);
  FrameSinkId d_id = FrameSinkId(5, 5);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Rect a_bounds_in_c = gfx::Rect(0, 0, 200, 100);
  gfx::Rect b_bounds_in_c = gfx::Rect(0, 0, 800, 600);
  gfx::Rect d_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c, transform_c_to_a,
      transform_c_to_b, transform_e_to_d;
  transform_e_to_c.Translate(-200, -150);
  transform_e_to_d.Translate(-400, -50);
  transform_c_to_b.Translate(0, -100);
  transform_c_to_b.Skew(2, 3);
  transform_c_to_b.Scale(.5f, .7f);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 4));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c_id, HitTestRegionFlags::kHitTestChildSurface,
                              c_bounds_in_e, transform_e_to_c, 2));  // c
  active_data_.push_back(AggregatedHitTestRegion(
      a_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      a_bounds_in_c, transform_c_to_a, 0));  // a
  active_data_.push_back(AggregatedHitTestRegion(
      b_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      b_bounds_in_c, transform_c_to_b, 0));  // b
  active_data_.push_back(AggregatedHitTestRegion(
      d_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      d_bounds_in_e, transform_e_to_d, 0));  // d
  SendHitTestData();

  // Check that we can get the correct transform to all regions.
  gfx::Transform transform_to_e;
  EXPECT_TRUE(hit_test_query().GetTransformToTarget(e_id, &transform_to_e));
  EXPECT_EQ(transform_to_e, gfx::Transform());

  gfx::Transform transform_to_c;
  gfx::Transform expected_transform_to_c;
  expected_transform_to_c.Translate(-200, -150);
  EXPECT_TRUE(hit_test_query().GetTransformToTarget(c_id, &transform_to_c));
  EXPECT_EQ(transform_to_c, expected_transform_to_c);

  gfx::Transform transform_to_a;
  gfx::Transform expected_transform_to_a;
  expected_transform_to_a.Translate(-200, -150);
  EXPECT_TRUE(hit_test_query().GetTransformToTarget(a_id, &transform_to_a));
  EXPECT_EQ(transform_to_a, expected_transform_to_a);

  gfx::Transform transform_to_b;
  gfx::Transform expected_transform_to_b;
  expected_transform_to_b.Translate(-200, -150);
  expected_transform_to_b.PostConcat(transform_c_to_b);
  EXPECT_TRUE(hit_test_query().GetTransformToTarget(b_id, &transform_to_b));
  // Use ToString so that we can compare float.
  EXPECT_EQ(transform_to_b.ToString(), expected_transform_to_b.ToString());

  gfx::Transform transform_to_d;
  gfx::Transform expected_transform_to_d;
  expected_transform_to_d.Translate(-400, -50);
  EXPECT_TRUE(hit_test_query().GetTransformToTarget(d_id, &transform_to_d));
  EXPECT_EQ(transform_to_d, expected_transform_to_d);
}

// Region c1 is transparent and on top of e, c2 is a child of e, d1 is a
// child of c1. Any events outside of d1 should go to either c2 or e instead
// of being taken by the transparent c1 region.
//
//  +e/c1----------+
//  |              |     Point   maps to
//  |   +c2-+      |     -----   -------
//  |   | 1 |      |       1        c2
//  |   +d1--------|       2        d1
//  |   | 2        |
//  |   |          |
//  +--------------+
//
TEST_F(HitTestQueryTest, TransparentOverlayRegions) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c1_id = FrameSinkId(2, 2);
  FrameSinkId c2_id = FrameSinkId(3, 3);
  FrameSinkId d1_id = FrameSinkId(4, 4);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c1_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c2_bounds_in_e = gfx::Rect(0, 0, 200, 100);
  gfx::Rect d1_bounds_in_c1 = gfx::Rect(0, 0, 400, 300);
  gfx::Transform transform_e_to_e, transform_e_to_c1, transform_e_to_c2,
      transform_c1_to_d1;
  transform_e_to_c2.Translate(-200, -100);
  transform_c1_to_d1.Translate(-200, -200);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 3));  // e
  active_data_.push_back(
      AggregatedHitTestRegion(c1_id,
                              HitTestRegionFlags::kHitTestChildSurface |
                                  HitTestRegionFlags::kHitTestMouse,
                              c1_bounds_in_e, transform_e_to_c1, 1));  // c1
  active_data_.push_back(AggregatedHitTestRegion(
      d1_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      d1_bounds_in_c1, transform_c1_to_d1, 0));  // d1
  active_data_.push_back(AggregatedHitTestRegion(
      c2_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c2_bounds_in_e, transform_e_to_c2, 0));  // c2
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(202, 102);
  gfx::PointF point2(202, 202);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, c2_id);
  EXPECT_EQ(target1.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, d1_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

TEST_F(HitTestQueryTest, FindTargetForLocationStartingFrom) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 200, 200);
  gfx::Transform transform_e_to_e, transform_e_to_c;
  transform_e_to_c.Translate(-100, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      e_bounds_in_e, transform_e_to_e, 1));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse,
      c_bounds_in_e, transform_e_to_c, 0));  // c
  SendHitTestData();

  // Test a point in the embedder with the embedder as the initial frame sink.
  gfx::PointF point1(99, 200);
  Target target1 = hit_test_query().FindTargetForLocationStartingFrom(
      EventSource::MOUSE, point1, e_id);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  // Test a point in the child with the embedder as the initial frame sink. The
  // point is still specified in the coordinate space of the embedder.
  gfx::PointF point2(150, 150);
  Target target2 = hit_test_query().FindTargetForLocationStartingFrom(
      EventSource::MOUSE, point2, e_id);
  EXPECT_EQ(target2.frame_sink_id, c_id);
  EXPECT_EQ(target2.location_in_target, gfx::PointF(50, 50));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  // Test a point in the child with the child as the initial frame sink. The
  // point is specified in the coordinate space of the child.
  gfx::PointF point3(40, 40);
  Target target3 = hit_test_query().FindTargetForLocationStartingFrom(
      EventSource::MOUSE, point3, c_id);
  EXPECT_EQ(target3.frame_sink_id, c_id);
  EXPECT_EQ(target3.location_in_target, point3);
  EXPECT_EQ(target3.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

// One embedder with nested OOPIFs. When the root view is overlapped we should
// continue checking its descendants rather than doing async hit test.
//
//  +e-------------+
//  |   +c---------|     Point   maps to
//  | 1 |    2     |     -----   -------
//  |   |          |       1        e
//  |   |          |       2        c
//  |   |          |
//  |   |          |
//  +--------------+
//
TEST_F(HitTestQueryTest, OverlappedRootView) {
  FrameSinkId e_id = FrameSinkId(1, 1);
  FrameSinkId c_id = FrameSinkId(2, 2);
  gfx::Rect e_bounds_in_e = gfx::Rect(0, 0, 600, 600);
  gfx::Rect c_bounds_in_e = gfx::Rect(0, 0, 800, 800);
  gfx::Transform transform_e_to_e, transform_e_to_c;
  transform_e_to_c.Translate(-200, -100);
  active_data_.push_back(AggregatedHitTestRegion(
      e_id,
      HitTestRegionFlags::kHitTestMine | HitTestRegionFlags::kHitTestMouse |
          HitTestRegionFlags::kHitTestAsk,
      e_bounds_in_e, transform_e_to_e, 1,
      AsyncHitTestReasons::kOverlappedRegion));  // e
  active_data_.push_back(AggregatedHitTestRegion(
      c_id,
      HitTestRegionFlags::kHitTestChildSurface | kHitTestMine |
          HitTestRegionFlags::kHitTestMouse,
      c_bounds_in_e, transform_e_to_c, 0));  // c
  SendHitTestData();

  // All points are in e's coordinate system when we reach this case.
  gfx::PointF point1(1, 1);
  gfx::PointF point2(202, 102);

  Target target1 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point1);
  EXPECT_EQ(target1.frame_sink_id, e_id);
  EXPECT_EQ(target1.location_in_target, point1);
  // kHitTestAsk should be dropped for overlapped root view.
  EXPECT_EQ(target1.flags, HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);

  Target target2 =
      hit_test_query().FindTargetForLocation(EventSource::MOUSE, point2);
  EXPECT_EQ(target2.frame_sink_id, c_id);
  // point2 + transform_e_to_c  = (2, 2).
  EXPECT_EQ(target2.location_in_target, gfx::PointF(2, 2));
  EXPECT_EQ(target2.flags, HitTestRegionFlags::kHitTestChildSurface |
                               HitTestRegionFlags::kHitTestMine |
                               HitTestRegionFlags::kHitTestMouse);
}

}  // namespace test
}  // namespace viz
