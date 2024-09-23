// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/linear_layout.h"

#include <memory>

#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {

// Helper class supplying convenient layout-related accessors.
class TestElement : public UiElement {
 public:
  float x() const { return world_space_transform().To2dTranslation().x(); }
  float y() const { return world_space_transform().To2dTranslation().y(); }
  float local_x() const { return LocalTransform().To2dTranslation().x(); }
  float local_y() const { return LocalTransform().To2dTranslation().y(); }
};

}  // namespace

TEST(LinearLayout, HorizontalVerticalLayout) {
  LinearLayout layout(LinearLayout::kRight);
  layout.set_margin(10);
  auto element = std::make_unique<UiElement>();
  UiElement* rect_a = element.get();
  rect_a->SetSize(10, 10);
  layout.AddChild(std::move(element));

  // One element should require no position adjustment at all.
  layout.SizeAndLayOut();
  EXPECT_TRUE(rect_a->LocalTransform().IsIdentity());

  // Two elements should be centered and separated by the margin.
  element = std::make_unique<UiElement>();
  UiElement* rect_b = element.get();
  rect_b->SetSize(10, 10);
  rect_b->SetScale(2.0f, 2.0f, 0.0f);
  layout.AddChild(std::move(element));
  layout.SizeAndLayOut();

  gfx::Point3F position_a;
  position_a = rect_a->LocalTransform().MapPoint(position_a);

  gfx::Point3F position_b;
  position_b = rect_b->LocalTransform().MapPoint(position_b);

  EXPECT_FLOAT_EQ(-15.0f, position_a.x());
  EXPECT_FLOAT_EQ(0.0f, position_a.y());
  EXPECT_FLOAT_EQ(0.0f, position_a.z());

  EXPECT_FLOAT_EQ(10.0f, position_b.x());
  EXPECT_FLOAT_EQ(0.0f, position_b.y());
  EXPECT_FLOAT_EQ(0.0f, position_b.z());

  rect_a->set_requires_layout(false);
  layout.SizeAndLayOut();

  EXPECT_FLOAT_EQ(20.0f, layout.size().width());
}

TEST(LinearLayout, Alignment) {
  LinearLayout layout(LinearLayout::kRight);
  layout.set_margin(10);
  auto element = std::make_unique<UiElement>();
  UiElement* rect_a = element.get();
  rect_a->SetSize(1, 1);
  layout.AddChild(std::move(element));
  element = std::make_unique<UiElement>();
  UiElement* rect_b = element.get();

  rect_b->SetSize(10, 10);
  rect_b->SetScale(2.0f, 2.0f, 0.0f);
  layout.AddChild(std::move(element));

  gfx::Point3F position_a;
  rect_a->set_y_anchoring(TOP);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(9.5f, position_a.y());
  position_a = gfx::Point3F();
  rect_a->set_y_anchoring(BOTTOM);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(-9.5f, position_a.y());

  layout.set_direction(LinearLayout::kLeft);
  position_a = gfx::Point3F();
  rect_a->set_y_anchoring(TOP);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(9.5f, position_a.y());
  position_a = gfx::Point3F();
  rect_a->set_y_anchoring(BOTTOM);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(-9.5f, position_a.y());

  layout.set_direction(LinearLayout::kDown);
  position_a = gfx::Point3F();
  rect_a->set_x_anchoring(LEFT);
  rect_a->set_y_anchoring(NONE);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(-9.5f, position_a.x());
  position_a = gfx::Point3F();
  rect_a->set_x_anchoring(RIGHT);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(9.5f, position_a.x());

  layout.set_direction(LinearLayout::kUp);
  position_a = gfx::Point3F();
  rect_a->set_x_anchoring(LEFT);
  rect_a->set_y_anchoring(NONE);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(-9.5f, position_a.x());
  position_a = gfx::Point3F();
  rect_a->set_x_anchoring(RIGHT);
  layout.SizeAndLayOut();
  position_a = rect_a->LocalTransform().MapPoint(position_a);
  EXPECT_FLOAT_EQ(9.5f, position_a.x());
}

TEST(LinearLayout, Orientations) {
  LinearLayout layout(LinearLayout::kUp);

  TestElement* rect;
  for (int i = 0; i < 2; i++) {
    auto element = std::make_unique<TestElement>();
    rect = element.get();
    element->SetSize(10, 10);
    layout.AddChild(std::move(element));
  }

  layout.set_direction(LinearLayout::kUp);
  layout.SizeAndLayOut();
  EXPECT_FLOAT_EQ(0.0f, rect->local_x());
  EXPECT_FLOAT_EQ(5.0f, rect->local_y());

  layout.set_direction(LinearLayout::kDown);
  layout.SizeAndLayOut();
  EXPECT_FLOAT_EQ(0.0f, rect->local_x());
  EXPECT_FLOAT_EQ(-5.0f, rect->local_y());

  layout.set_direction(LinearLayout::kLeft);
  layout.SizeAndLayOut();
  EXPECT_FLOAT_EQ(-5.0f, rect->local_x());
  EXPECT_FLOAT_EQ(0.0f, rect->local_y());

  layout.set_direction(LinearLayout::kRight);
  layout.SizeAndLayOut();
  EXPECT_FLOAT_EQ(5.0f, rect->local_x());
  EXPECT_FLOAT_EQ(0.0f, rect->local_y());
}

TEST(LinearLayout, NestedLayouts) {
  // Build a tree of elements, including nested layouts:
  //   parent_layout
  //     child_layout
  //       rect_a
  //       rect_b
  //     rect_c
  auto parent_layout = std::make_unique<LinearLayout>(LinearLayout::kDown);
  UiElement* p_parent_layout = parent_layout.get();
  auto child_layout = std::make_unique<LinearLayout>(LinearLayout::kDown);
  UiElement* p_child_layout = child_layout.get();
  auto rect_a = std::make_unique<TestElement>();
  TestElement* p_rect_a = rect_a.get();
  rect_a->SetSize(10, 10);
  child_layout->AddChild(std::move(rect_a));
  auto rect_b = std::make_unique<TestElement>();
  TestElement* p_rect_b = rect_b.get();
  rect_b->SetSize(10, 10);
  child_layout->AddChild(std::move(rect_b));
  auto rect_c = std::make_unique<TestElement>();
  TestElement* p_rect_c = rect_c.get();
  rect_c->SetSize(999, 10);
  parent_layout->AddChild(std::move(child_layout));
  parent_layout->AddChild(std::move(rect_c));

  auto scene = std::make_unique<UiScene>();
  scene->AddUiElement(kRoot, std::move(parent_layout));
  scene->OnBeginFrame(gfx::MicrosecondsToTicks(1), kStartHeadPose);

  // Ensure that layouts expand to include the cumulative size of children.
  EXPECT_FLOAT_EQ(p_parent_layout->size().width(), 999.f);
  EXPECT_FLOAT_EQ(p_parent_layout->size().height(), 30.f);
  EXPECT_FLOAT_EQ(p_child_layout->size().width(), 10.f);
  EXPECT_FLOAT_EQ(p_child_layout->size().height(), 20.f);

  // Ensure that children are at correct positions.
  EXPECT_FLOAT_EQ(p_rect_a->x(), 0);
  EXPECT_FLOAT_EQ(p_rect_a->y(), 10);
  EXPECT_FLOAT_EQ(p_rect_b->x(), 0);
  EXPECT_FLOAT_EQ(p_rect_b->y(), 0);
  EXPECT_FLOAT_EQ(p_rect_c->x(), 0);
  EXPECT_FLOAT_EQ(p_rect_c->y(), -10);
}

}  // namespace vr
