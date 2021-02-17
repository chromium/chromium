// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/repositioner.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

constexpr float kTestContentDistance = 2.0f;

struct TestCase {
  TestCase(gfx::Vector3dF v, gfx::Vector3dF u, gfx::Point3F p, gfx::Vector3dF r)
      : laser_direction(v),
        head_up_vector(u),
        expected_element_center(p),
        expected_right_vector(r) {}
  gfx::Vector3dF laser_direction;
  gfx::Vector3dF head_up_vector;
  gfx::Point3F expected_element_center;
  gfx::Vector3dF expected_right_vector;
  bool enabled = true;
};

void CheckRepositionedCorrectly(const TestCase& test_case) {
  UiScene scene;
  auto child = std::make_unique<UiElement>();
  child->SetTranslate(0, 0, -kTestContentDistance);
  auto* element = child.get();
  auto parent = std::make_unique<Repositioner>();
  auto* repositioner = parent.get();

  parent->AddChild(std::move(child));
  scene.AddUiElement(kRoot, std::move(parent));

  repositioner->set_laser_direction(kForwardVector);
  repositioner->SetEnabled(test_case.enabled);

  gfx::Transform head_pose(
      gfx::Quaternion({0, 1, 0}, test_case.head_up_vector));

  repositioner->set_laser_direction(test_case.laser_direction);
  scene.OnBeginFrame(MsToTicks(0), head_pose);
  repositioner->SetEnabled(false);

  gfx::Point3F center = element->GetCenter();
  EXPECT_NEAR(center.x(), test_case.expected_element_center.x(), kEpsilon);
  EXPECT_NEAR(center.y(), test_case.expected_element_center.y(), kEpsilon);
  EXPECT_NEAR(center.z(), test_case.expected_element_center.z(), kEpsilon);
  gfx::Vector3dF right_vector = {1, 0, 0};
  element->world_space_transform().TransformVector(&right_vector);
  EXPECT_VECTOR3DF_NEAR(right_vector, test_case.expected_right_vector,
                        kEpsilon);
}

}  // namespace

TEST(Repositioner, RepositionNegativeZWithReticle) {
  std::vector<TestCase> test_cases = {
      // Move reticle, up is not adjusted.
      {{1, 0, 0}, {0, 1, 0}, {kTestContentDistance, 0, 0}, {0, 0, 1}},
      // Should snap as head position is within threshold of up.
      {{0, 0, -1}, {0, 0.9f, 0}, {0, 0, -kTestContentDistance}, {1, 0, 0}},
      // If the user's head is leaning far back, we do tilt.
      {{0, 1, 0},
       {0.1f, 0.1f, 0.8f},
       {0, kTestContentDistance, 0},
       {-0.992278f, 0, 0.124035f}},
  };

  for (size_t i = 0; i < test_cases.size(); i++) {
    TestCase test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Test case index = %zd (enabled)", i));
    CheckRepositionedCorrectly(test_case);

    // Before enabling the repositioner, child element should NOT have rotation.
    test_case.enabled = false;
    test_case.expected_right_vector = {1, 0, 0};
    test_case.expected_element_center = {0, 0, -kTestContentDistance};

    SCOPED_TRACE(base::StringPrintf("Test case index = %zd (disabled)", i));
    CheckRepositionedCorrectly(test_case);
  }
}

}  // namespace vr
