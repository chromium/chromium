// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/viewport_aware_root.h"

#include <cmath>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

namespace {

const float kThreshold = ViewportAwareRoot::kViewportRotationTriggerDegrees;
const int kFramesPerSecond = 60;

bool Point3FAreNearlyEqual(const gfx::Point3F& lhs, const gfx::Point3F& rhs) {
  return base::IsApproximatelyEqual(lhs.x(), rhs.x(), kEpsilon) &&
         base::IsApproximatelyEqual(lhs.y(), rhs.y(), kEpsilon) &&
         base::IsApproximatelyEqual(lhs.z(), rhs.z(), kEpsilon);
}

bool MatricesAreNearlyEqual(const gfx::Transform& lhs,
                            const gfx::Transform& rhs) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (!base::IsApproximatelyEqual<double>(lhs.rc(row, col),
                                              rhs.rc(row, col), kEpsilon)) {
        return false;
      }
    }
  }
  return true;
}

void RotateAboutYAxis(float degrees, gfx::Vector3dF* out) {
  gfx::Transform transform;
  transform.RotateAboutYAxis(degrees);
  *out = transform.MapVector(*out);
}

class ViewportAwareRootForTesting : public ViewportAwareRoot {
 public:
  ViewportAwareRootForTesting() = default;
  ~ViewportAwareRootForTesting() override = default;

  bool AdjustRotationForHeadPose(const gfx::Vector3dF& look_at) override {
    return ViewportAwareRoot::AdjustRotationForHeadPose(look_at);
  }
};

}  // namespace

class ViewportAwareRootTest : public testing::Test {
 public:
  ViewportAwareRootTest() = default;
  ~ViewportAwareRootTest() override = default;

  void SetUp() override {
    scene_ = std::make_unique<UiScene>();
    auto viewport_aware_root = std::make_unique<ViewportAwareRootForTesting>();
    viewport_aware_root->SetDrawPhase(kPhaseForeground);
    viewport_root = viewport_aware_root.get();
    scene_->AddUiElement(kRoot, std::move(viewport_aware_root));

    auto element = std::make_unique<UiElement>();
    element->SetDrawPhase(kPhaseForeground);
    element->SetTranslate(0.f, 0.f, -1.f);
    viewport_element = element.get();
    viewport_root->AddChild(std::move(element));
  }

 protected:
  void CheckRotateClockwiseAndReverse(const gfx::Vector3dF& initial_look_at) {
    SCOPED_TRACE(initial_look_at.ToString());
    gfx::Vector3dF look_at(initial_look_at);
    gfx::Transform expected;

    viewport_root->AdjustRotationForHeadPose(look_at);
    EXPECT_TRUE(
        MatricesAreNearlyEqual(expected, viewport_root->LocalTransform()));

    float total_rotation = 0.f;
    RotateAboutYAxis(kThreshold - 1.f, &look_at);
    total_rotation += (kThreshold - 1.f);
    viewport_root->AdjustRotationForHeadPose(look_at);
    // Rotating less than kThreshold should yield identity local transform.
    EXPECT_TRUE(
        MatricesAreNearlyEqual(expected, viewport_root->LocalTransform()));

    // Rotate look_at clockwise again kThreshold degrees.
    RotateAboutYAxis(kThreshold, &look_at);
    total_rotation += kThreshold;
    viewport_root->AdjustRotationForHeadPose(look_at);

    // Rotating more than kThreshold should reposition to element to center.
    // We have rotated kThreshold - 1 + kThreshold degrees in total.
    expected.RotateAboutYAxis(total_rotation);
    EXPECT_TRUE(
        MatricesAreNearlyEqual(expected, viewport_root->LocalTransform()));

    // Since we rotated, reset total rotation.
    total_rotation = 0.f;

    // Rotate look_at counter clockwise kThreshold-2 degrees.
    RotateAboutYAxis(-(kThreshold - 2.f), &look_at);
    total_rotation -= (kThreshold - 2.f);
    viewport_root->AdjustRotationForHeadPose(look_at);
    // Rotating opposite direction within the kThreshold should not reposition.
    EXPECT_TRUE(
        MatricesAreNearlyEqual(expected, viewport_root->LocalTransform()));

    // Rotate look_at counter clockwise again kThreshold degrees.
    RotateAboutYAxis(-kThreshold, &look_at);
    total_rotation -= kThreshold;
    viewport_root->AdjustRotationForHeadPose(look_at);
    expected.RotateAboutYAxis(total_rotation);
    // Rotating opposite direction passing the kThreshold should reposition.
    EXPECT_TRUE(
        MatricesAreNearlyEqual(expected, viewport_root->LocalTransform()));

    viewport_root->Reset();
  }

  bool AnimateWithForwardVector(base::TimeDelta delta,
                                const gfx::Vector3dF& forward_vector) {
    base::TimeTicks target_time = current_time_ + delta;
    base::TimeDelta frame_duration = base::Seconds(1.0 / kFramesPerSecond);
    bool changed = false;
    gfx::Quaternion head_movement_quat(forward_vector, {0.f, 0.f, -1.f});
    gfx::Transform head_pose(head_movement_quat);
    for (; current_time_ < target_time; current_time_ += frame_duration) {
      if (scene_->OnBeginFrame(current_time_, head_pose))
        changed = true;
    }
    current_time_ = target_time;
    if (scene_->OnBeginFrame(current_time_, head_pose))
      changed = true;
    return changed;
  }

  raw_ptr<ViewportAwareRootForTesting, DanglingUntriaged> viewport_root;
  raw_ptr<UiElement, DanglingUntriaged> viewport_element;

 private:
  std::unique_ptr<UiScene> scene_;
  base::TimeTicks current_time_;
};

TEST_F(ViewportAwareRootTest, TestAdjustRotationForHeadPose) {
  CheckRotateClockwiseAndReverse(gfx::Vector3dF{0.f, 0.f, -1.f});

  CheckRotateClockwiseAndReverse(
      gfx::Vector3dF{0.f, std::sin(1.0f), -std::cos(1.0f)});

  CheckRotateClockwiseAndReverse(
      gfx::Vector3dF{0.f, -std::sin(1.5f), -std::cos(1.5f)});
}

TEST_F(ViewportAwareRootTest, ChildElementsRepositioned) {
  gfx::Vector3dF look_at{0.f, 0.f, -1.f};
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(0), look_at));
  EXPECT_TRUE(Point3FAreNearlyEqual(gfx::Point3F(0.f, 0.f, -1.f),
                                    viewport_element->GetCenter()));

  // This should trigger reposition of viewport aware elements.
  RotateAboutYAxis(90.f, &look_at);
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(10), look_at));
  EXPECT_TRUE(Point3FAreNearlyEqual(gfx::Point3F(-1.f, 0.f, 0.f),
                                    viewport_element->GetCenter()));
}

TEST_F(ViewportAwareRootTest, ChildElementsHasOpacityAnimation) {
  gfx::Vector3dF look_at{0.f, 0.f, -1.f};
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(0), look_at));
  EXPECT_TRUE(viewport_element->IsVisible());

  // Trigger a reposition.
  RotateAboutYAxis(90.f, &look_at);
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(5), look_at));

  // Initially the element should be invisible and then animate to its full
  // opacity.
  EXPECT_FALSE(viewport_element->IsVisible());
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(50), look_at));
  EXPECT_TRUE(viewport_element->IsVisible());
  EXPECT_TRUE(IsAnimating(viewport_root, {OPACITY}));
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(500), look_at));
  EXPECT_TRUE(viewport_element->IsVisible());
  EXPECT_FALSE(IsAnimating(viewport_root, {OPACITY}));
}

TEST_F(ViewportAwareRootTest, ResetPositionWhenReshow) {
  gfx::Vector3dF look_at{0.f, 0.f, -1.f};
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(0), look_at));
  gfx::Point3F original_position = viewport_element->GetCenter();

  RotateAboutYAxis(90.f, &look_at);
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(10), look_at));

  RotateAboutYAxis(-60.f, &look_at);
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(20), look_at));
  // Viewport element should have 90 - 60 = 30 degrees rotation.
  EXPECT_TRUE(Point3FAreNearlyEqual(gfx::Point3F(-0.5f, 0.f, -0.866025f),
                                    viewport_element->GetCenter()));

  // Hide all children and then reshow later should reset children's position.
  viewport_element->SetVisibleImmediately(false);
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(30), look_at));
  viewport_element->SetVisibleImmediately(true);
  EXPECT_TRUE(AnimateWithForwardVector(gfx::MsToDelta(40), look_at));
  EXPECT_TRUE(
      Point3FAreNearlyEqual(original_position, viewport_element->GetCenter()));
}

TEST_F(ViewportAwareRootTest, IsChildrenVisible) {
  auto element = std::make_unique<UiElement>();
  element->SetDrawPhase(kPhaseNone);
  UiElement* child = element.get();
  viewport_element->AddChild(std::move(element));
  viewport_element->SetDrawPhase(kPhaseNone);
  // root
  //   viewport_element  -> no draw, visible
  //     child           -> no draw, visible
  EXPECT_FALSE(viewport_root->HasVisibleChildren());

  child->SetDrawPhase(kPhaseForeground);
  // root
  //   viewport_element  -> no draw, visible
  //     child           -> drawable, visible
  EXPECT_TRUE(viewport_root->HasVisibleChildren());

  // Visible non draw parent  should not be considered as visible children.
  child->SetVisibleImmediately(false);
  // root
  //   viewport_element  -> no draw, visible
  //     child           -> drawable, invisible
  EXPECT_FALSE(viewport_root->HasVisibleChildren());

  child->SetVisibleImmediately(true);
  viewport_element->SetDrawPhase(kPhaseForeground);
  // root
  //   viewport_element  -> drawable, visible
  //     child           -> drawable, visible
  EXPECT_TRUE(viewport_root->HasVisibleChildren());

  viewport_element->SetDrawPhase(kPhaseForeground);
  viewport_element->SetVisibleImmediately(false);
  // root
  //   viewport_element  -> drawable, invisible
  //     child           -> drawable, visible
  EXPECT_FALSE(viewport_root->HasVisibleChildren());
}

}  // namespace vr
