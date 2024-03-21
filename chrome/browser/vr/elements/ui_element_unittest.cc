// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <utility>

#include "base/functional/bind.h"
#include "cc/animation/keyframe_model.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace vr {

TEST(UiElement, BoundsContainChildren) {
  auto parent = std::make_unique<UiElement>();
  parent->set_bounds_contain_children(true);
  parent->set_padding(0.1, 0.2);

  auto c1 = std::make_unique<UiElement>();
  c1->SetSize(3.0f, 3.0f);
  c1->SetTranslate(2.5f, 2.5f, 0.0f);
  auto* c1_ptr = c1.get();
  parent->AddChild(std::move(c1));

  parent->SizeAndLayOut();
  EXPECT_RECTF_NEAR(gfx::RectF(2.5f, 2.5f, 3.2f, 3.4f),
                    gfx::RectF(parent->local_origin(), parent->size()),
                    kEpsilon);
  EXPECT_EQ(parent->GetCenter().ToString(), c1_ptr->GetCenter().ToString());

  auto c2 = std::make_unique<UiElement>();
  c2->SetSize(4.0f, 4.0f);
  c2->SetTranslate(-3.0f, 0.0f, 0.0f);
  parent->AddChild(std::move(c2));

  parent->SizeAndLayOut();
  EXPECT_RECTF_NEAR(gfx::RectF(-0.5f, 1.0f, 9.2f, 6.4f),
                    gfx::RectF(parent->local_origin(), parent->size()),
                    kEpsilon);

  auto c3 = std::make_unique<UiElement>();
  c3->SetSize(2.0f, 2.0f);
  c3->SetTranslate(0.0f, -2.0f, 0.0f);
  parent->AddChild(std::move(c3));

  parent->SizeAndLayOut();
  EXPECT_RECTF_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
                    gfx::RectF(parent->local_origin(), parent->size()),
                    kEpsilon);

  auto c4 = std::make_unique<UiElement>();
  c4->SetSize(2.0f, 2.0f);
  c4->SetTranslate(20.0f, 20.0f, 0.0f);
  c4->SetVisible(false);
  parent->AddChild(std::move(c4));

  // We expect no change due to an invisible child.
  parent->SizeAndLayOut();
  EXPECT_RECTF_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
                    gfx::RectF(parent->local_origin(), parent->size()),
                    kEpsilon);

  auto grand_parent = std::make_unique<UiElement>();
  grand_parent->set_bounds_contain_children(true);
  grand_parent->set_padding(0.1, 0.2);
  grand_parent->AddChild(std::move(parent));

  auto anchored = std::make_unique<UiElement>();
  anchored->set_y_anchoring(BOTTOM);
  anchored->set_contributes_to_parent_bounds(false);

  auto* anchored_ptr = anchored.get();
  grand_parent->AddChild(std::move(anchored));

  grand_parent->SizeAndLayOut();
  EXPECT_RECTF_NEAR(
      gfx::RectF(-0.5f, 0.5f, 9.4f, 7.8f),
      gfx::RectF(grand_parent->local_origin(), grand_parent->size()), kEpsilon);

  gfx::Point3F p;
  p = anchored_ptr->LocalTransform().MapPoint(p);
  EXPECT_FLOAT_EQ(-3.9, p.y());
}

TEST(UiElement, ReplaceChild) {
  auto a = std::make_unique<UiElement>();
  auto b = std::make_unique<UiElement>();
  auto c = std::make_unique<UiElement>();
  auto d = std::make_unique<UiElement>();
  auto x = std::make_unique<UiElement>();

  auto* x_ptr = x.get();
  auto* c_ptr = c.get();

  a->AddChild(std::move(b));
  a->AddChild(std::move(c));
  a->AddChild(std::move(d));

  auto removed = a->ReplaceChild(c_ptr, std::move(x));

  EXPECT_EQ(c_ptr, removed.get());
  EXPECT_EQ(x_ptr, a->children()[1].get());
}

TEST(UiElement, IgnoringAsymmetricPadding) {
  // This test ensures that when we ignore asymmetric padding that we don't
  // accidentally shift the location of the parent; it should stay put.
  auto a = std::make_unique<UiElement>();
  a->set_bounds_contain_children(true);

  auto b = std::make_unique<UiElement>();
  b->set_bounds_contain_children(true);
  b->set_bounds_contain_padding(false);
  b->set_padding(0.0f, 5.0f, 0.0f, 0.0f);

  auto c = std::make_unique<UiElement>();
  c->set_bounds_contain_children(true);
  c->set_bounds_contain_padding(false);
  c->set_padding(0.0f, 2.0f, 0.0f, 0.0f);

  auto d = std::make_unique<UiElement>();
  d->SetSize(0.5f, 0.5f);

  c->AddChild(std::move(d));
  c->SizeAndLayOut();
  b->AddChild(std::move(c));
  b->SizeAndLayOut();
  a->AddChild(std::move(b));
  a->SizeAndLayOut();

  a->UpdateWorldSpaceTransform(false);

  gfx::Point3F p;
  p = a->world_space_transform().MapPoint(p);

  EXPECT_POINT3F_EQ(gfx::Point3F(), p);
}

TEST(UiElement, BoundsContainPaddingWithAnchoring) {
  // If an element's bounds do not contain padding, then padding should be
  // discounted when doing anchoring.
  auto parent = std::make_unique<UiElement>();
  parent->SetSize(1.0, 1.0);

  auto child = std::make_unique<UiElement>();
  child->SetSize(0.5, 0.5);
  child->set_padding(2.0, 2.0);
  child->set_bounds_contain_padding(false);
  child->set_bounds_contain_children(true);

  auto* child_ptr = child.get();

  parent->AddChild(std::move(child));

  struct {
    LayoutAlignment x_anchoring;
    LayoutAlignment y_anchoring;
    gfx::Point3F expected_position;
  } test_cases[] = {
      {LEFT, NONE, {-0.5, 0, 0}},
      {RIGHT, NONE, {0.5, 0, 0}},
      {NONE, TOP, {0, 0.5, 0}},
      {NONE, BOTTOM, {0, -0.5, 0}},
  };

  for (auto test_case : test_cases) {
    child_ptr->set_contributes_to_parent_bounds(false);
    child_ptr->set_x_anchoring(test_case.x_anchoring);
    child_ptr->set_y_anchoring(test_case.y_anchoring);
    parent->SizeAndLayOut();
    gfx::Point3F p;
    p = child_ptr->LocalTransform().MapPoint(p);
    EXPECT_POINT3F_EQ(test_case.expected_position, p);
  }
}

TEST(UiElement, BoundsContainPaddingWithCentering) {
  // If an element's bounds do not contain padding, then padding should be
  // discounted when doing centering.
  auto parent = std::make_unique<UiElement>();
  parent->SetSize(1.0, 1.0);

  auto child = std::make_unique<UiElement>();
  child->set_padding(2.0, 2.0);
  child->set_bounds_contain_padding(false);
  child->set_bounds_contain_children(true);

  auto grandchild = std::make_unique<UiElement>();
  grandchild->SetSize(0.5, 0.5);

  child->AddChild(std::move(grandchild));

  auto* child_ptr = child.get();

  parent->AddChild(std::move(child));

  struct {
    LayoutAlignment x_centering;
    LayoutAlignment y_centering;
    gfx::Point3F expected_position;
  } test_cases[] = {
      {LEFT, NONE, {0.25, 0, 0}},
      {RIGHT, NONE, {-0.25, 0, 0}},
      {NONE, TOP, {0, -0.25, 0}},
      {NONE, BOTTOM, {0, 0.25, 0}},
  };

  for (auto test_case : test_cases) {
    child_ptr->set_contributes_to_parent_bounds(false);
    child_ptr->set_x_centering(test_case.x_centering);
    child_ptr->set_y_centering(test_case.y_centering);
    parent->SizeAndLayOut();
    EXPECT_POINT3F_EQ(test_case.expected_position,
                      child_ptr->LocalTransform().MapPoint(gfx::Point3F()));
  }
}

TEST(UiElement, BoundsContainScaledChildren) {
  auto a = std::make_unique<UiElement>();
  a->SetSize(0.4, 0.3);

  auto b = std::make_unique<UiElement>();
  b->SetSize(0.2, 0.2);
  b->SetTranslate(0.6, 0.0, 0.0);
  b->SetScale(2.0, 3.0, 1.0);

  auto c = std::make_unique<UiElement>();
  c->set_bounds_contain_children(true);
  c->set_padding(0.05, 0.05);
  c->AddChild(std::move(a));
  c->AddChild(std::move(b));

  c->SizeAndLayOut();
  EXPECT_RECTF_NEAR(gfx::RectF(0.3f, 0.0f, 1.1f, 0.7f),
                    gfx::RectF(c->local_origin(), c->size()), kEpsilon);
}

TEST(UiElement, AnimateSize) {
  UiScene scene;
  scene.RunFirstFrameForTest();
  auto rect = std::make_unique<UiElement>();
  rect->SetSize(10, 100);
  rect->AddKeyframeModel(gfx::CreateSizeAnimation(
      rect.get(), 1, BOUNDS, gfx::SizeF(10, 100), gfx::SizeF(20, 200),
      gfx::MicrosecondsToDelta(10000)));
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));
  base::TimeTicks start_time = gfx::MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kStartHeadPose));
  EXPECT_SIZEF_EQ(gfx::SizeF(10, 100), rect_ptr->size());
  EXPECT_TRUE(scene.OnBeginFrame(start_time + gfx::MicrosecondsToDelta(10000),
                                 kStartHeadPose));
  EXPECT_SIZEF_EQ(gfx::SizeF(20, 200), rect_ptr->size());
}

TEST(UiElement, AnimationAffectsInheritableTransform) {
  UiScene scene;
  scene.RunFirstFrameForTest();
  auto rect = std::make_unique<UiElement>();
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));

  gfx::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  gfx::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  rect_ptr->AddKeyframeModel(gfx::CreateTransformAnimation(
      rect_ptr, 2, TRANSFORM, from_operations, to_operations,
      gfx::MicrosecondsToDelta(10000)));

  base::TimeTicks start_time = gfx::MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kStartHeadPose));
  EXPECT_POINT3F_EQ(gfx::Point3F(10, 100, 1000),
                    rect_ptr->LocalTransform().MapPoint(gfx::Point3F()));
  EXPECT_TRUE(scene.OnBeginFrame(start_time + gfx::MicrosecondsToDelta(10000),
                                 kStartHeadPose));
  EXPECT_POINT3F_EQ(gfx::Point3F(20, 200, 2000),
                    rect_ptr->LocalTransform().MapPoint(gfx::Point3F()));
}

TEST(UiElement, CoordinatedVisibilityTransitions) {
  UiScene scene;
  bool value = false;

  auto parent = std::make_unique<UiElement>();
  auto* parent_ptr = parent.get();
  parent->SetVisible(false);
  parent->SetTransitionedProperties({OPACITY});
  parent->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA([](bool* value) { return *value; },
                     base::Unretained(&value)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          parent_ptr)));

  auto child = std::make_unique<UiElement>();
  auto* child_ptr = child.get();
  child->SetVisible(false);
  child->SetTransitionedProperties({OPACITY});
  child->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA([](bool* value) { return *value; },
                     base::Unretained(&value)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          child_ptr)));

  parent->AddChild(std::move(child));
  scene.AddUiElement(kRoot, std::move(parent));

  scene.OnBeginFrame(gfx::MsToTicks(0), kStartHeadPose);

  value = true;

  scene.OnBeginFrame(gfx::MsToTicks(16), kStartHeadPose);

  // We should have started animating both, and they should both be at opacity
  // zero given that this is the first frame. This does not guarantee that
  // they've started animating together, however. Even if the animation was
  // unticked, we would still be at opacity zero. We must tick a second time to
  // reach a non-zero value.
  EXPECT_TRUE(parent_ptr->IsAnimatingProperty(OPACITY));
  EXPECT_TRUE(child_ptr->IsAnimatingProperty(OPACITY));
  EXPECT_EQ(child_ptr->opacity(), parent_ptr->opacity());

  scene.OnBeginFrame(gfx::MsToTicks(32), kStartHeadPose);
  EXPECT_EQ(child_ptr->opacity(), parent_ptr->opacity());
  EXPECT_LT(0.0f, child_ptr->opacity());
}

}  // namespace vr
