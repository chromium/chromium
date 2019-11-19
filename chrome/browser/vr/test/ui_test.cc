// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_test.h"

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

namespace {

gfx::Vector3dF ComputeNormal(const gfx::Transform& transform) {
  gfx::Vector3dF x_axis(1, 0, 0);
  gfx::Vector3dF y_axis(0, 1, 0);
  transform.TransformVector(&x_axis);
  transform.TransformVector(&y_axis);
  gfx::Vector3dF normal = CrossProduct(x_axis, y_axis);
  normal.GetNormalized(&normal);
  return normal;
}

bool WillElementFaceCamera(const UiElement* element) {
  // Element might become invisible due to incorrect rotation, i.e when rotation
  // cause the visible side of the element flip.
  // Here we calculate the dot product of (origin - center) and normal. If the
  // result is greater than 0, it means the visible side of this element is
  // facing camera.
  gfx::Point3F center;
  gfx::Transform transform = element->ComputeTargetWorldSpaceTransform();
  transform.TransformPoint(&center);

  gfx::Point3F origin;
  gfx::Vector3dF normal = ComputeNormal(transform);
  if (center == origin) {
    // If the center of element is at origin, as our camera facing negative z.
    // we only need to make sure the normal of the element have positive z.
    return normal.z() > 0.f;
  }
  return gfx::DotProduct(origin - center, normal) > 0.f;
}

// This method tests whether an element will be visible after all pending scene
// animations complete.
bool WillElementBeVisible(const UiElement* element) {
  if (!element)
    return false;
  if (element->ComputeTargetOpacity() == 0.f)
    return false;
  if (!element->IsWorldPositioned())
    return true;
  return WillElementFaceCamera(element);
}

int NumVisibleInTreeRecursive(const UiElement* element) {
  int visible = WillElementBeVisible(element) ? 1 : 0;
  for (auto& child : element->children())
    visible += NumVisibleInTreeRecursive(child.get());
  return visible;
}

}  // namespace

UiTest::UiTest() {}
UiTest::~UiTest() {}

void UiTest::SetUp() {
  browser_ = std::make_unique<testing::NiceMock<MockUiBrowserInterface>>();
}

void UiTest::CreateSceneInternal(
    const UiInitialState& state,
    std::unique_ptr<MockContentInputDelegate> content_input_delegate) {
  content_input_delegate_ = content_input_delegate.get();
  ui_instance_ = std::make_unique<Ui>(std::move(browser_.get()),
                                      std::move(content_input_delegate),
                                      nullptr, nullptr, nullptr, state);
  ui_ = ui_instance_.get();
  scene_ = ui_instance_->scene();
  model_ = ui_instance_->model_for_test();
  model_->controllers[0].transform.Translate3d(kStartControllerPosition);

  OnBeginFrame();
  // Need a second BeginFrame here because the first one will add controllers
  // to the scene, which need an additional frame to get into a good state.
  OnBeginFrame();
}

void UiTest::CreateScene(const UiInitialState& state) {
  auto content_input_delegate =
      std::make_unique<testing::NiceMock<MockContentInputDelegate>>();
  CreateSceneInternal(state, std::move(content_input_delegate));
}

void UiTest::CreateScene(InWebVr in_web_vr) {
  UiInitialState state;
  state.in_web_vr = in_web_vr;
  CreateScene(state);
}

void UiTest::SetIncognito(bool incognito) {
  model_->incognito = incognito;
}

bool UiTest::IsVisible(UiElementName name) const {
  OnBeginFrame();
  UiElement* element = scene_->GetUiElementByName(name);
  return WillElementBeVisible(element);
}

bool UiTest::VerifyVisibility(const std::set<UiElementName>& names,
                              bool expected_visibility) const {
  OnBeginFrame();
  for (auto name : names) {
    SCOPED_TRACE(UiElementNameToString(name));
    UiElement* element = scene_->GetUiElementByName(name);
    bool will_be_visible = WillElementBeVisible(element);
    EXPECT_EQ(will_be_visible, expected_visibility);
    if (will_be_visible != expected_visibility)
      return false;
  }
  return true;
}

void UiTest::VerifyOnlyElementsVisible(
    const std::string& trace_context,
    const std::set<UiElementName>& names) const {
  OnBeginFrame();
  SCOPED_TRACE(trace_context);
  for (auto* element : scene_->GetAllElements()) {
    SCOPED_TRACE(element->DebugName());
    UiElementName name = element->name();
    UiElementName owner_name = element->owner_name_for_test();
    if (element->draw_phase() == kPhaseNone && owner_name == kNone) {
      EXPECT_TRUE(names.find(name) == names.end());
      continue;
    }
    if (name == kNone)
      name = owner_name;
    bool should_be_visible = (names.find(name) != names.end());
    EXPECT_EQ(WillElementBeVisible(element), should_be_visible);
  }
}

int UiTest::NumVisibleInTree(UiElementName name) const {
  OnBeginFrame();
  auto* root = scene_->GetUiElementByName(name);
  EXPECT_NE(root, nullptr);
  if (!root) {
    return 0;
  }
  return NumVisibleInTreeRecursive(root);
}

bool UiTest::VerifyIsAnimating(const std::set<UiElementName>& names,
                               const std::vector<TargetProperty>& properties,
                               bool animating) const {
  OnBeginFrame();
  for (auto name : names) {
    auto* element = scene_->GetUiElementByName(name);
    EXPECT_NE(nullptr, element);
    SCOPED_TRACE(element->DebugName());
    if (IsAnimating(element, properties) != animating) {
      return false;
    }
  }
  return true;
}

bool UiTest::VerifyRequiresLayout(const std::set<UiElementName>& names,
                                  bool requires_layout) const {
  OnBeginFrame();
  for (auto name : names) {
    SCOPED_TRACE(name);
    auto* element = scene_->GetUiElementByName(name);
    EXPECT_NE(nullptr, element);
    if (!element || element->requires_layout() != requires_layout) {
      return false;
    }
  }
  return true;
}

bool UiTest::RunForMs(float milliseconds) {
  return RunFor(base::TimeDelta::FromMilliseconds(milliseconds));
}

bool UiTest::RunForSeconds(float seconds) {
  return RunFor(base::TimeDelta::FromSecondsD(seconds));
}

bool UiTest::AdvanceFrame() {
  current_time_ += base::TimeDelta::FromMilliseconds(16);
  return OnBeginFrame();
}

void UiTest::GetBackgroundColor(SkColor* background_color) const {
  OnBeginFrame();
  Rect* background =
      static_cast<Rect*>(scene_->GetUiElementByName(kSolidBackground));
  ASSERT_NE(nullptr, background);
  EXPECT_EQ(background->center_color(), background->edge_color());
  *background_color = background->edge_color();
}

void UiTest::ClickElement(UiElement* element) {
  // Synthesize a controller vector targeting the element.
  gfx::Point3F target;
  element->ComputeTargetWorldSpaceTransform().TransformPoint(&target);
  gfx::Point3F origin;
  gfx::Vector3dF direction(target - origin);
  direction.GetNormalized(&direction);

  RenderInfo render_info;
  ReticleModel reticle_model;
  InputEventList input_event_list;
  ControllerModel controller_model;
  controller_model.laser_direction = direction;
  controller_model.laser_origin = origin;

  controller_model.touchpad_button_state = ControllerModel::ButtonState::kDown;
  ui_instance_->input_manager()->HandleInput(current_time_, render_info,
                                             controller_model, &reticle_model,
                                             &input_event_list);
  OnBeginFrame();

  controller_model.touchpad_button_state = ControllerModel::ButtonState::kUp;
  ui_instance_->input_manager()->HandleInput(current_time_, render_info,
                                             controller_model, &reticle_model,
                                             &input_event_list);
  OnBeginFrame();
}

bool UiTest::RunFor(base::TimeDelta delta) {
  base::TimeTicks target_time = current_time_ + delta;
  base::TimeDelta frame_time = base::TimeDelta::FromMilliseconds(16);
  bool changed = false;

  // Run a frame in the near future to trigger new state changes.
  current_time_ += frame_time;
  changed |= OnBeginFrame();

  // If needed, skip ahead and run another frame at the target time.
  if (current_time_ < target_time) {
    current_time_ = target_time;
    changed |= OnBeginFrame();
  }

  return changed;
}

bool UiTest::OnBeginFrame() const {
  bool changed = false;
  model_->current_time = current_time_;
  changed |= scene_->OnBeginFrame(current_time_, kStartHeadPose);
  if (scene_->HasDirtyTextures()) {
    scene_->UpdateTextures();
    changed = true;
  }
  return changed;
}

}  // namespace vr
