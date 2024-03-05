// Copyright 2017 The Chromium Authors
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
  gfx::Vector3dF x_axis = transform.MapVector(gfx::Vector3dF(1, 0, 0));
  gfx::Vector3dF y_axis = transform.MapVector(gfx::Vector3dF(0, 1, 0));
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
  gfx::Transform transform = element->ComputeTargetWorldSpaceTransform();
  gfx::Point3F center = transform.MapPoint(gfx::Point3F());

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

}  // namespace

UiTest::UiTest() {}
UiTest::~UiTest() {}

void UiTest::SetUp() {
  ui_instance_ = std::make_unique<Ui>();
  ui_ = ui_instance_.get();
  scene_ = ui_instance_->scene();
  model_ = ui_instance_->model_for_test();

  OnBeginFrame();
  // Need a second BeginFrame here because the first one will add controllers
  // to the scene, which need an additional frame to get into a good state.
  OnBeginFrame();
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
    // TODO(https://crbug.com/327467653): Timeout Spinner only visible on
    // Windows.
#if !BUILDFLAG(IS_WIN)
    if (name == kWebVrTimeoutSpinner) {
      will_be_visible = expected_visibility;
    }
#endif
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
  for (vr::UiElement* element : scene_->GetAllElements()) {
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

bool UiTest::RunForMs(float milliseconds) {
  return RunFor(base::Milliseconds(milliseconds));
}

bool UiTest::RunForSeconds(float seconds) {
  return RunFor(base::Seconds(seconds));
}

bool UiTest::AdvanceFrame() {
  current_time_ += base::Milliseconds(16);
  return OnBeginFrame();
}

bool UiTest::RunFor(base::TimeDelta delta) {
  base::TimeTicks target_time = current_time_ + delta;
  base::TimeDelta frame_time = base::Milliseconds(16);
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
  changed |= scene_->OnBeginFrame(current_time_, kStartHeadPose);
  if (scene_->HasDirtyTextures()) {
    scene_->UpdateTextures();
    changed = true;
  }
  return changed;
}

}  // namespace vr
