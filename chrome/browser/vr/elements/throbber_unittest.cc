// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/throbber.h"

#include <memory>

#include "cc/animation/transform_operation.h"
#include "cc/animation/transform_operations.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {
constexpr float kTestSize = 2.0;
constexpr float kInitialScale = 2.0;
constexpr float kInitialOpacity = 0.8;
}  // namespace

TEST(Throbber, CircleGrowAnimation) {
  UiScene scene;
  scene.RunFirstFrameForTest();
  auto element = std::make_unique<Throbber>();
  element->SetSize(kTestSize, kTestSize);
  element->SetCornerRadius(kTestSize / 2);
  element->SetScale(kInitialScale, kInitialScale, kInitialScale);
  element->SetOpacity(kInitialOpacity);
  Throbber* throbber = element.get();
  scene.AddUiElement(kRoot, std::move(element));

  throbber->SetCircleGrowAnimationEnabled(true);
  scene.OnBeginFrame(MsToTicks(1), kStartHeadPose);
  EXPECT_TRUE(throbber->IsAnimatingProperty(CIRCLE_GROW));

  // Half way through animation.
  scene.OnBeginFrame(MsToTicks(501), kStartHeadPose);
  EXPECT_FLOAT_EQ(throbber->opacity(), kInitialOpacity / 2);
  EXPECT_FLOAT_EQ(
      throbber->GetTargetTransform().at(UiElement::kScaleIndex).scale.x,
      kInitialScale * 1.5);
  EXPECT_FLOAT_EQ(
      throbber->GetTargetTransform().at(UiElement::kScaleIndex).scale.y,
      kInitialScale * 1.5);
  EXPECT_FLOAT_EQ(
      throbber->GetTargetTransform().at(UiElement::kScaleIndex).scale.z,
      kInitialScale);

  // Reset to initial value.
  throbber->SetCircleGrowAnimationEnabled(false);
  EXPECT_FLOAT_EQ(throbber->opacity(), kInitialOpacity);
  EXPECT_FLOAT_EQ(
      throbber->GetTargetTransform().at(UiElement::kScaleIndex).scale.x,
      kInitialScale);
  EXPECT_FLOAT_EQ(
      throbber->GetTargetTransform().at(UiElement::kScaleIndex).scale.y,
      kInitialScale);
  EXPECT_FLOAT_EQ(
      throbber->GetTargetTransform().at(UiElement::kScaleIndex).scale.z,
      kInitialScale);
}

}  // namespace vr
