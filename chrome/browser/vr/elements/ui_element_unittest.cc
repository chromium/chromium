// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "cc/animation/keyframe_model.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {
constexpr float kAlmostOne = 0.999f;
}

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
  EXPECT_RECT_NEAR(gfx::RectF(2.5f, 2.5f, 3.2f, 3.4f),
                   gfx::RectF(parent->local_origin(), parent->size()),
                   kEpsilon);
  EXPECT_EQ(parent->GetCenter().ToString(), c1_ptr->GetCenter().ToString());

  auto c2 = std::make_unique<UiElement>();
  c2->SetSize(4.0f, 4.0f);
  c2->SetTranslate(-3.0f, 0.0f, 0.0f);
  parent->AddChild(std::move(c2));

  parent->SizeAndLayOut();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 1.0f, 9.2f, 6.4f),
                   gfx::RectF(parent->local_origin(), parent->size()),
                   kEpsilon);

  auto c3 = std::make_unique<UiElement>();
  c3->SetSize(2.0f, 2.0f);
  c3->SetTranslate(0.0f, -2.0f, 0.0f);
  parent->AddChild(std::move(c3));

  parent->SizeAndLayOut();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
                   gfx::RectF(parent->local_origin(), parent->size()),
                   kEpsilon);

  auto c4 = std::make_unique<UiElement>();
  c4->SetSize(2.0f, 2.0f);
  c4->SetTranslate(20.0f, 20.0f, 0.0f);
  c4->SetVisible(false);
  parent->AddChild(std::move(c4));

  // We expect no change due to an invisible child.
  parent->SizeAndLayOut();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
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
  EXPECT_RECT_NEAR(
      gfx::RectF(-0.5f, 0.5f, 9.4f, 7.8f),
      gfx::RectF(grand_parent->local_origin(), grand_parent->size()), kEpsilon);

  gfx::Point3F p;
  anchored_ptr->LocalTransform().TransformPoint(&p);
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
  a->world_space_transform().TransformPoint(&p);

  EXPECT_VECTOR3DF_EQ(gfx::Point3F(), p);
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
    child_ptr->LocalTransform().TransformPoint(&p);
    EXPECT_VECTOR3DF_EQ(test_case.expected_position, p);
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
    gfx::Point3F p;
    child_ptr->LocalTransform().TransformPoint(&p);
    EXPECT_VECTOR3DF_EQ(test_case.expected_position, p);
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
  EXPECT_RECT_NEAR(gfx::RectF(0.3f, 0.0f, 1.1f, 0.7f),
                   gfx::RectF(c->local_origin(), c->size()), kEpsilon);
}

TEST(UiElement, AnimateSize) {
  UiScene scene;
  scene.RunFirstFrameForTest();
  auto rect = std::make_unique<UiElement>();
  rect->SetSize(10, 100);
  rect->AddKeyframeModel(CreateBoundsAnimation(1, 1, gfx::SizeF(10, 100),
                                               gfx::SizeF(20, 200),
                                               MicrosecondsToDelta(10000)));
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));
  base::TimeTicks start_time = MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kStartHeadPose));
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(10, 100), rect_ptr->size());
  EXPECT_TRUE(scene.OnBeginFrame(start_time + MicrosecondsToDelta(10000),
                                 kStartHeadPose));
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(20, 200), rect_ptr->size());
}

TEST(UiElement, AnimationAffectsInheritableTransform) {
  UiScene scene;
  scene.RunFirstFrameForTest();
  auto rect = std::make_unique<UiElement>();
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  rect_ptr->AddKeyframeModel(CreateTransformAnimation(
      2, 2, from_operations, to_operations, MicrosecondsToDelta(10000)));

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kStartHeadPose));
  gfx::Point3F p;
  rect_ptr->LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(10, 100, 1000), p);
  p = gfx::Point3F();
  EXPECT_TRUE(scene.OnBeginFrame(start_time + MicrosecondsToDelta(10000),
                                 kStartHeadPose));
  rect_ptr->LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(20, 200, 2000), p);
}

TEST(UiElement, HitTest) {
  UiElement rect;
  rect.SetSize(1.0, 1.0);

  UiElement circle;
  circle.SetSize(1.0, 1.0);
  circle.SetCornerRadius(1.0 / 2);

  UiElement rounded_rect;
  rounded_rect.SetSize(1.0, 0.5);
  rounded_rect.SetCornerRadius(0.2);

  struct {
    gfx::PointF location;
    bool expected_rect;
    bool expected_circle;
    bool expected_rounded_rect;
  } test_cases[] = {
      // Walk left edge
      {gfx::PointF(0.f, 0.1f), true, false, false},
      {gfx::PointF(0.f, 0.45f), true, false, true},
      {gfx::PointF(0.f, 0.55f), true, false, true},
      {gfx::PointF(0.f, 0.95f), true, false, false},
      {gfx::PointF(0.f, kAlmostOne), true, false, false},
      // Walk bottom edge
      {gfx::PointF(0.1f, kAlmostOne), true, false, false},
      {gfx::PointF(0.45f, kAlmostOne), true, false, true},
      {gfx::PointF(0.55f, kAlmostOne), true, false, true},
      {gfx::PointF(0.95f, kAlmostOne), true, false, false},
      {gfx::PointF(kAlmostOne, kAlmostOne), true, false, false},
      // Walk right edge
      {gfx::PointF(kAlmostOne, 0.95f), true, false, false},
      {gfx::PointF(kAlmostOne, 0.55f), true, false, true},
      {gfx::PointF(kAlmostOne, 0.45f), true, false, true},
      {gfx::PointF(kAlmostOne, 0.1f), true, false, false},
      {gfx::PointF(kAlmostOne, 0.f), true, false, false},
      // Walk top edge
      {gfx::PointF(0.95f, 0.f), true, false, false},
      {gfx::PointF(0.55f, 0.f), true, false, true},
      {gfx::PointF(0.45f, 0.f), true, false, true},
      {gfx::PointF(0.1f, 0.f), true, false, false},
      {gfx::PointF(0.f, 0.f), true, false, false},
      // center
      {gfx::PointF(0.5f, 0.5f), true, true, true},
      // A point which is included in rounded rect but not in cicle.
      {gfx::PointF(0.1f, 0.1f), true, false, true},
      // An invalid point.
      {gfx::PointF(-0.1f, -0.1f), false, false, false},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(test_cases[i].expected_rect,
              rect.LocalHitTest(test_cases[i].location));
    EXPECT_EQ(test_cases[i].expected_circle,
              circle.LocalHitTest(test_cases[i].location));
    EXPECT_EQ(test_cases[i].expected_rounded_rect,
              rounded_rect.LocalHitTest(test_cases[i].location));
  }
}

TEST(UiElement, HitTestWithClip) {
  UiElement rect;
  rect.SetSize(1.0, 1.0);
  // A horizontal band in the middle.
  rect.SetClipRect({0.0f, 0.3f, 1.0f, 0.4f});
  struct {
    gfx::PointF location;
    bool expected;
  } test_cases[] = {
      // Vertical walk.
      {{0.5f, 0.0f}, false},
      {{0.5f, 0.2f}, false},
      {{0.5f, 0.4f}, true},
      {{0.5f, 0.6f}, true},
      {{0.5f, 0.8f}, false},
      {{0.5f, 1.0f}, false},
      // Horizontal walk.
      {{0.0f, 0.5f}, true},
      {{0.2f, 0.5f}, true},
      {{0.4f, 0.5f}, true},
      {{0.6f, 0.5f}, true},
      {{0.8f, 0.5f}, true},
      {{kAlmostOne, 0.5f}, true},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(test_cases[i].expected,
              rect.LocalHitTest(test_cases[i].location));
  }
}

class ElementEventHandlers {
 public:
  explicit ElementEventHandlers(UiElement* element) {
    DCHECK(element);
    EventHandlers event_handlers;
    event_handlers.hover_enter = base::BindRepeating(
        &ElementEventHandlers::HandleHoverEnter, base::Unretained(this));
    event_handlers.hover_move = base::BindRepeating(
        &ElementEventHandlers::HandleHoverMove, base::Unretained(this));
    event_handlers.hover_leave = base::BindRepeating(
        &ElementEventHandlers::HandleHoverLeave, base::Unretained(this));
    event_handlers.button_down = base::BindRepeating(
        &ElementEventHandlers::HandleButtonDown, base::Unretained(this));
    event_handlers.button_up = base::BindRepeating(
        &ElementEventHandlers::HandleButtonUp, base::Unretained(this));
    element->set_event_handlers(event_handlers);
  }
  void HandleHoverEnter() { hover_enter_ = true; }
  bool hover_enter_called() { return hover_enter_; }

  void HandleHoverMove(const gfx::PointF& position) { hover_move_ = true; }
  bool hover_move_called() { return hover_move_; }

  void HandleHoverLeave() { hover_leave_ = true; }
  bool hover_leave_called() { return hover_leave_; }

  void HandleButtonDown() { button_down_ = true; }
  bool button_down_called() { return button_down_; }

  void HandleButtonUp() { button_up_ = true; }
  bool button_up_called() { return button_up_; }

  void ExpectCalled(bool called) {
    EXPECT_EQ(hover_enter_called(), called);
    EXPECT_EQ(hover_move_called(), called);
    EXPECT_EQ(hover_leave_called(), called);
    EXPECT_EQ(button_down_called(), called);
    EXPECT_EQ(button_up_called(), called);
  }

 private:
  bool hover_enter_ = false;
  bool hover_move_ = false;
  bool hover_leave_ = false;
  bool button_up_ = false;
  bool button_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(ElementEventHandlers);
};

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

  scene.OnBeginFrame(MsToTicks(0), kStartHeadPose);

  value = true;

  scene.OnBeginFrame(MsToTicks(16), kStartHeadPose);

  // We should have started animating both, and they should both be at opacity
  // zero given that this is the first frame. This does not guarantee that
  // they've started animating together, however. Even if the animation was
  // unticked, we would still be at opacity zero. We must tick a second time to
  // reach a non-zero value.
  EXPECT_TRUE(parent_ptr->IsAnimatingProperty(OPACITY));
  EXPECT_TRUE(child_ptr->IsAnimatingProperty(OPACITY));
  EXPECT_EQ(child_ptr->opacity(), parent_ptr->opacity());

  scene.OnBeginFrame(MsToTicks(32), kStartHeadPose);
  EXPECT_EQ(child_ptr->opacity(), parent_ptr->opacity());
  EXPECT_LT(0.0f, child_ptr->opacity());
}

TEST(UiElement, EventBubbling) {
  auto element = std::make_unique<UiElement>();
  auto child = std::make_unique<UiElement>();
  auto grand_child = std::make_unique<UiElement>();
  auto* child_ptr = child.get();
  auto* grand_child_ptr = grand_child.get();
  child->AddChild(std::move(grand_child));
  element->AddChild(std::move(child));

  // Add event handlers to element and child.
  ElementEventHandlers element_handlers(element.get());
  ElementEventHandlers child_handlers(child_ptr);

  // Events on grand_child don't bubble up the parent chain.
  grand_child_ptr->OnHoverEnter(gfx::PointF(), base::TimeTicks());
  grand_child_ptr->OnHoverMove(gfx::PointF(), base::TimeTicks());
  grand_child_ptr->OnHoverLeave(base::TimeTicks());
  grand_child_ptr->OnButtonDown(gfx::PointF(), base::TimeTicks());
  grand_child_ptr->OnButtonUp(gfx::PointF(), base::TimeTicks());
  child_handlers.ExpectCalled(false);
  element_handlers.ExpectCalled(false);

  // Events on grand_child bubble up the parent chain.
  grand_child_ptr->set_bubble_events(true);
  grand_child_ptr->OnHoverEnter(gfx::PointF(), base::TimeTicks());
  grand_child_ptr->OnHoverMove(gfx::PointF(), base::TimeTicks());
  grand_child_ptr->OnHoverLeave(base::TimeTicks());
  grand_child_ptr->OnButtonDown(gfx::PointF(), base::TimeTicks());
  grand_child_ptr->OnButtonUp(gfx::PointF(), base::TimeTicks());
  child_handlers.ExpectCalled(true);
  // Events don't bubble to element since it doesn't have the bubble_events bit
  // set.
  element_handlers.ExpectCalled(false);
}

// The clip rect is properly transformed into the child's coordinates.
TEST(UiElement, ClipChildren) {
  auto parent = std::make_unique<UiElement>();
  parent->SetSize(16.0f, 8.0f);
  parent->set_clip_descendants(true);
  auto child = std::make_unique<UiElement>();
  child->SetSize(4.0f, 4.0f);
  child->set_contributes_to_parent_bounds(false);
  child->set_y_anchoring(TOP);
  auto* p_child = child.get();
  parent->AddChild(std::move(child));

  parent->SizeAndLayOut();

  EXPECT_FLOAT_RECT_EQ(gfx::RectF(-1.5f, 0.5f, 4.0f, 2.0f),
                       p_child->GetClipRect());

  p_child->SetScale(0.5f, 0.5f, 1.0f);
  parent->SizeAndLayOut();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(-3.5f, 0.5f, 8.0f, 4.0f),
                       p_child->GetClipRect());
}

}  // namespace vr
