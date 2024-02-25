// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#include <numbers>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/values.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/viewport_aware_root.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

#define TOLERANCE 0.0001

#define EXPECT_VEC3F_NEAR(a, b)         \
  EXPECT_NEAR(a.x(), b.x(), TOLERANCE); \
  EXPECT_NEAR(a.y(), b.y(), TOLERANCE); \
  EXPECT_NEAR(a.z(), b.z(), TOLERANCE);

namespace vr {

namespace {

size_t NumElementsInSubtree(UiElement* element) {
  size_t count = 1;
  for (auto& child : element->children()) {
    count += NumElementsInSubtree(child.get());
  }
  return count;
}

class AlwaysDirty : public UiElement {
 public:
  ~AlwaysDirty() override {}

  bool OnBeginFrame(const gfx::Transform& head_pose) override { return true; }
};

}  // namespace

TEST(UiScene, AddRemoveElements) {
  UiScene scene;

  // Always start with the root element.
  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 1u);

  auto element = std::make_unique<UiElement>();
  element->SetDrawPhase(kPhaseForeground);
  UiElement* parent = element.get();
  int parent_id = parent->id();
  scene.AddUiElement(kRoot, std::move(element));

  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 2u);

  element = std::make_unique<UiElement>();
  element->SetDrawPhase(kPhaseForeground);
  UiElement* child = element.get();
  int child_id = child->id();

  parent->AddChild(std::move(element));

  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 3u);

  EXPECT_NE(scene.GetUiElementById(parent_id), nullptr);
  EXPECT_NE(scene.GetUiElementById(child_id), nullptr);
  EXPECT_EQ(scene.GetUiElementById(-1), nullptr);

  auto removed_child = scene.RemoveUiElement(child_id);
  EXPECT_EQ(removed_child.get(), child);
  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 2u);
  EXPECT_EQ(scene.GetUiElementById(child_id), nullptr);

  auto removed_parent = scene.RemoveUiElement(parent_id);
  EXPECT_EQ(removed_parent.get(), parent);
  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 1u);
  EXPECT_EQ(scene.GetUiElementById(parent_id), nullptr);
}

TEST(UiScene, IsVisibleInHiddenSubtree) {
  UiScene scene;

  // Always start with the root element.
  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 1u);

  auto element = std::make_unique<UiElement>();
  element->SetDrawPhase(kPhaseForeground);
  UiElement* parent = element.get();
  scene.AddUiElement(kRoot, std::move(element));

  element = std::make_unique<UiElement>();
  element->SetDrawPhase(kPhaseForeground);
  UiElement* child = element.get();

  parent->AddChild(std::move(element));

  // Set initial computed opacity.
  scene.OnBeginFrame(gfx::MsToTicks(1), kStartHeadPose);

  parent->SetVisible(false);

  scene.OnBeginFrame(gfx::MsToTicks(2), kStartHeadPose);

  // On the second walk, we should skip the child.
  scene.OnBeginFrame(gfx::MsToTicks(3), kStartHeadPose);

  EXPECT_FALSE(child->IsVisible());
}

// This test creates a parent and child UI element, each with their own
// transformations, and ensures that the child's computed total transform
// incorporates the parent's transform as well as its own.
TEST(UiScene, ParentTransformAppliesToChild) {
  UiScene scene;

  // Add a parent element, with distinct transformations.
  // Size of the parent should be ignored by the child.
  auto element = std::make_unique<UiElement>();
  UiElement* parent = element.get();
  element->SetSize(1000, 1000);

  element->SetTranslate(6, 1, 0);
  element->SetRotate(0, 0, 1, 0.5f * std::numbers::pi_v<float>);
  element->SetScale(3, 3, 1);
  scene.AddUiElement(kRoot, std::move(element));

  // Add a child to the parent, with different transformations.
  element = std::make_unique<UiElement>();
  element->SetTranslate(3, 0, 0);
  element->SetRotate(0, 0, 1, 0.5f * std::numbers::pi_v<float>);
  element->SetScale(2, 2, 1);
  UiElement* child = element.get();
  parent->AddChild(std::move(element));

  scene.OnBeginFrame(gfx::MsToTicks(0), kStartHeadPose);
  gfx::Point3F origin = child->world_space_transform().MapPoint(gfx::Point3F());
  gfx::Point3F point =
      child->world_space_transform().MapPoint(gfx::Point3F(1, 0, 0));
  EXPECT_VEC3F_NEAR(gfx::Point3F(6, 10, 0), origin);
  EXPECT_VEC3F_NEAR(gfx::Point3F(0, 10, 0), point);
}

TEST(UiScene, Opacity) {
  UiScene scene;

  auto element = std::make_unique<UiElement>();
  UiElement* parent = element.get();
  element->SetOpacity(0.5);
  scene.AddUiElement(kRoot, std::move(element));

  element = std::make_unique<UiElement>();
  UiElement* child = element.get();
  element->SetOpacity(0.5);
  parent->AddChild(std::move(element));

  scene.OnBeginFrame(gfx::MsToTicks(0), kStartHeadPose);
  EXPECT_EQ(0.5f, parent->computed_opacity());
  EXPECT_EQ(0.25f, child->computed_opacity());
}

TEST(UiScene, NoViewportAwareElementWhenNoVisibleChild) {
  UiScene scene;
  auto element = std::make_unique<UiElement>();
  UiElement* container = element.get();
  element->SetName(kWebVrRoot);
  scene.AddUiElement(kRoot, std::move(element));

  auto root = std::make_unique<ViewportAwareRoot>();
  UiElement* viewport_aware_root = root.get();
  container->AddChild(std::move(root));

  element = std::make_unique<UiElement>();
  UiElement* child = element.get();
  element->SetDrawPhase(kPhaseOverlayForeground);
  viewport_aware_root->AddChild(std::move(element));

  element = std::make_unique<UiElement>();
  element->SetDrawPhase(kPhaseOverlayForeground);
  child->AddChild(std::move(element));

  EXPECT_FALSE(scene.GetWebVrOverlayElementsToDraw().empty());
  child->SetVisible(false);
  scene.OnBeginFrame(gfx::MsToTicks(0), kStartHeadPose);
  EXPECT_TRUE(scene.GetWebVrOverlayElementsToDraw().empty());
}

TEST(UiScene, InvisibleElementsDoNotCauseAnimationDirtiness) {
  UiScene scene;
  auto element = std::make_unique<UiElement>();
  element->AddKeyframeModel(gfx::CreateColorAnimation(
      element.get(), 1, BACKGROUND_COLOR, SK_ColorBLACK, SK_ColorWHITE,
      gfx::MsToDelta(1000)));
  UiElement* element_ptr = element.get();
  scene.AddUiElement(kRoot, std::move(element));
  EXPECT_TRUE(scene.OnBeginFrame(gfx::MsToTicks(1), kStartHeadPose));

  element_ptr->SetVisible(false);
  element_ptr->UpdateComputedOpacity();
  EXPECT_FALSE(scene.OnBeginFrame(gfx::MsToTicks(2), kStartHeadPose));
}

TEST(UiScene, InvisibleElementsDoNotCauseBindingDirtiness) {
  UiScene scene;
  auto element = std::make_unique<UiElement>();
  struct FakeModel {
    int foo = 1;
  } model;
  element->AddBinding(VR_BIND(int, FakeModel, &model, model->foo, UiElement,
                              element.get(), view->SetSize(1, value)));
  UiElement* element_ptr = element.get();
  scene.AddUiElement(kRoot, std::move(element));
  EXPECT_TRUE(scene.OnBeginFrame(gfx::MsToTicks(1), kStartHeadPose));

  model.foo = 2;
  element_ptr->SetVisible(false);
  element_ptr->UpdateComputedOpacity();
  EXPECT_FALSE(scene.OnBeginFrame(gfx::MsToTicks(2), kStartHeadPose));
}

TEST(UiScene, InvisibleElementsDoNotCauseOnBeginFrameDirtiness) {
  UiScene scene;
  auto element = std::make_unique<AlwaysDirty>();
  UiElement* element_ptr = element.get();
  scene.AddUiElement(kRoot, std::move(element));
  EXPECT_TRUE(scene.OnBeginFrame(gfx::MsToTicks(1), kStartHeadPose));

  element_ptr->SetVisible(false);
  element_ptr->UpdateComputedOpacity();
  EXPECT_FALSE(scene.OnBeginFrame(gfx::MsToTicks(2), kStartHeadPose));
}

typedef struct {
  LayoutAlignment x_anchoring;
  LayoutAlignment y_anchoring;
  LayoutAlignment x_centering;
  LayoutAlignment y_centering;
  float expected_x;
  float expected_y;
} AlignmentTestCase;

class AlignmentTest : public ::testing::TestWithParam<AlignmentTestCase> {};

TEST_P(AlignmentTest, VerifyCorrectPosition) {
  UiScene scene;

  // Create a parent element with non-unity size and scale.
  auto element = std::make_unique<UiElement>();
  UiElement* parent = element.get();
  element->SetSize(2, 2);
  element->SetScale(2, 2, 1);
  scene.AddUiElement(kRoot, std::move(element));

  // Add a child to the parent, with anchoring.
  element = std::make_unique<UiElement>();
  UiElement* child = element.get();
  element->SetSize(1, 1);
  element->set_contributes_to_parent_bounds(false);
  element->set_x_anchoring(GetParam().x_anchoring);
  element->set_y_anchoring(GetParam().y_anchoring);
  element->set_x_centering(GetParam().x_centering);
  element->set_y_centering(GetParam().y_centering);
  parent->AddChild(std::move(element));

  scene.OnBeginFrame(gfx::MsToTicks(0), kStartHeadPose);
  EXPECT_NEAR(GetParam().expected_x, child->GetCenter().x(), TOLERANCE);
  EXPECT_NEAR(GetParam().expected_y, child->GetCenter().y(), TOLERANCE);
}

const std::vector<AlignmentTestCase> alignment_test_cases = {
    // Test anchoring.
    {NONE, NONE, NONE, NONE, 0, 0},
    {LEFT, NONE, NONE, NONE, -2, 0},
    {RIGHT, NONE, NONE, NONE, 2, 0},
    {NONE, TOP, NONE, NONE, 0, 2},
    {NONE, BOTTOM, NONE, NONE, 0, -2},
    {LEFT, TOP, NONE, NONE, -2, 2},
    // Test centering.
    {NONE, NONE, LEFT, NONE, 1, 0},
    {NONE, NONE, RIGHT, NONE, -1, 0},
    {NONE, NONE, NONE, TOP, 0, -1},
    {NONE, NONE, NONE, BOTTOM, 0, 1},
    {NONE, NONE, LEFT, TOP, 1, -1},
    // Test a combination of the two.
    {RIGHT, TOP, LEFT, BOTTOM, 3, 3},
};

INSTANTIATE_TEST_SUITE_P(AlignmentTestCases,
                         AlignmentTest,
                         ::testing::ValuesIn(alignment_test_cases));

}  // namespace vr
