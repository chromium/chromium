// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/scaled_depth_adjuster.h"

#include <memory>

#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

void CheckScaleAndDepth(UiElement* element, float s) {
  EXPECT_POINT3F_EQ(gfx::Point3F(0, 0, -s), element->GetCenter());
  gfx::Point3F x(1.0f, 0, 0);
  element->world_space_transform().TransformPoint(&x);
  EXPECT_POINT3F_EQ(gfx::Point3F(s, 0, -s), x);
}

// This test confirms that an element is both positioned the right distance from
// the origin and that the inherited scale is correct (should match the distance
// in magnitude).
TEST(ScaledDepthAdjuster, SimpleDepth) {
  UiScene scene;
  auto element = std::make_unique<UiElement>();
  auto* p_element = element.get();
  auto adjuster = std::make_unique<ScaledDepthAdjuster>(2.5);
  adjuster->AddChild(std::move(element));
  scene.AddUiElement(kRoot, std::move(adjuster));
  scene.OnBeginFrame(MsToTicks(0), kStartHeadPose);
  CheckScaleAndDepth(p_element, 2.5);
}

// This test confirms that depth and scale adjustments work correctly if nested.
// Constructs a scene that appears as follows:
// kRoot
//   grandparent scaler (2.5)
//     grandparent
//       parent scaler(-.1)
//         parent
//           child scaler(.2)
//             child
TEST(ScaledDepthAdjuster, InheritedDepth) {
  UiScene scene;
  auto child = std::make_unique<UiElement>();
  auto* p_child = child.get();
  auto child_adjuster = std::make_unique<ScaledDepthAdjuster>(0.2f);

  auto parent = std::make_unique<UiElement>();
  auto* p_parent = parent.get();
  auto parent_adjuster = std::make_unique<ScaledDepthAdjuster>(-0.1f);

  auto grandparent = std::make_unique<UiElement>();
  auto* p_grandparent = grandparent.get();
  auto grandparent_adjuster = std::make_unique<ScaledDepthAdjuster>(2.5f);

  child_adjuster->AddChild(std::move(child));
  parent->AddChild(std::move(child_adjuster));
  parent_adjuster->AddChild(std::move(parent));
  grandparent->AddChild(std::move(parent_adjuster));
  grandparent_adjuster->AddChild(std::move(grandparent));
  scene.AddUiElement(kRoot, std::move(grandparent_adjuster));
  scene.OnBeginFrame(MsToTicks(0), kStartHeadPose);

  CheckScaleAndDepth(p_child, 2.6f);
  CheckScaleAndDepth(p_parent, 2.4f);
  CheckScaleAndDepth(p_grandparent, 2.5f);
}

}  // namespace vr
