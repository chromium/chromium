// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iomanip>
#include <queue>
#include <sstream>
#include <string>
#include <utility>

#include "chrome/browser/vr/ui.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/angle_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/skia_surface_provider_factory.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

namespace {

constexpr float kMargin = base::DegToRad(1.0f);

UiElementName UserFriendlyElementNameToUiElementName(
    UserFriendlyElementName name) {
  switch (name) {
    case UserFriendlyElementName::kWebXrAudioIndicator:
      return kWebVrAudioCaptureIndicator;
    case UserFriendlyElementName::kMicrophonePermissionIndicator:
      return kAudioCaptureIndicator;
    case UserFriendlyElementName::kWebXrExternalPromptNotification:
      return kWebXrExternalPromptNotification;
    case UserFriendlyElementName::kCameraPermissionIndicator:
      return kVideoCaptureIndicator;
    case UserFriendlyElementName::kLocationPermissionIndicator:
      return kLocationAccessIndicator;
    case UserFriendlyElementName::kWebXrLocationPermissionIndicator:
      return kWebVrLocationAccessIndicator;
    case UserFriendlyElementName::kWebXrVideoPermissionIndicator:
      return kWebVrVideoCaptureIndicator;
    default:
      NOTREACHED_IN_MIGRATION();
      return kNone;
  }
}

}  // namespace

Ui::Ui()
    : scene_(std::make_unique<UiScene>()), model_(std::make_unique<Model>()) {
  model_->web_vr.has_received_permissions = false;
  model_->web_vr.state = kWebVrAwaitingFirstFrame;

  UiSceneCreator(scene_.get(), this, model_.get()).CreateScene();
}

Ui::~Ui() = default;

base::WeakPtr<BrowserUiInterface> Ui::GetBrowserUiWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

SchedulerUiInterface* Ui::GetSchedulerUiPtr() {
  return this;
}

void Ui::SetCapturingState(const CapturingStateModel& active_capturing,
                           const CapturingStateModel& background_capturing,
                           const CapturingStateModel& potential_capturing) {
  model_->active_capturing = active_capturing;
  model_->background_capturing = background_capturing;
  model_->potential_capturing = potential_capturing;
  model_->web_vr.has_received_permissions = true;
}

void Ui::OnGlInitialized() {
  ui_element_renderer_ = std::make_unique<UiElementRenderer>();
  ui_renderer_ =
      std::make_unique<UiRenderer>(scene_.get(), ui_element_renderer_.get());
  provider_ = SkiaSurfaceProviderFactory::Create();
  scene_->OnGlInitialized(provider_.get());
}

void Ui::OnWebXrFrameAvailable() {
  model_->web_vr.state = kWebVrPresenting;
}

void Ui::OnWebXrTimeoutImminent() {
  model_->web_vr.state = kWebVrTimeoutImminent;
}

void Ui::OnWebXrTimedOut() {
  model_->web_vr.state = kWebVrTimedOut;
}

bool Ui::GetElementVisibility(UserFriendlyElementName element_name) {
  auto* target_element = scene()->GetUiElementByName(
      UserFriendlyElementNameToUiElementName(element_name));
  DCHECK(target_element) << "Unsupported test element";
  return target_element->IsVisible();
}

gfx::Point3F Ui::GetTargetPointForTesting(UserFriendlyElementName element_name,
                                          const gfx::PointF& position) {
  auto* target_element = scene()->GetUiElementByName(
      UserFriendlyElementNameToUiElementName(element_name));
  DCHECK(target_element) << "Unsupported test element";
  // The position to click is provided for a unit square, so scale it to match
  // the actual element.
  auto scaled_position = ScalePoint(position, target_element->size().width(),
                                    target_element->size().height());
  gfx::Point3F target =
      target_element->ComputeTargetWorldSpaceTransform().MapPoint(
          gfx::Point3F(scaled_position));
  // We do hit testing with respect to the eye position (world origin), so we
  // need to project the target point into the background.
  gfx::Vector3dF direction = target - kOrigin;
  direction.GetNormalized(&direction);
  return kOrigin +
         gfx::ScaleVector3d(direction, scene()->background_distance());
}

void Ui::SetVisibleExternalPromptNotification(
    ExternalPromptNotificationType prompt) {
  model_->web_vr.external_prompt_notification = prompt;
}

bool Ui::OnBeginFrame(base::TimeTicks current_time,
                      const gfx::Transform& head_pose) {
  return scene_->OnBeginFrame(current_time, head_pose);
}

bool Ui::SceneHasDirtyTextures() const {
  return scene_->HasDirtyTextures();
}

void Ui::UpdateSceneTextures() {
  scene_->UpdateTextures();
}

void Ui::Draw(const vr::RenderInfo& info) {
  ui_renderer_->Draw(info);
}

void Ui::DrawWebVrOverlayForeground(const vr::RenderInfo& info) {
  ui_renderer_->DrawWebVrOverlayForeground(info);
}

bool Ui::HasWebXrOverlayElementsToDraw() {
  return scene_->HasWebXrOverlayElementsToDraw();
}

std::pair<FovRectangle, FovRectangle> Ui::GetMinimalFovForWebXrOverlayElements(
    const gfx::Transform& left_view,
    const FovRectangle& fov_recommended_left,
    const gfx::Transform& right_view,
    const FovRectangle& fov_recommended_right,
    float z_near) {
  auto elements = scene_->GetWebVrOverlayElementsToDraw();
  return {GetMinimalFov(left_view, elements, fov_recommended_left, z_near),
          GetMinimalFov(right_view, elements, fov_recommended_right, z_near)};
}

FovRectangle Ui::GetMinimalFov(const gfx::Transform& view_matrix,
                               const std::vector<const UiElement*>& elements,
                               const FovRectangle& fov_recommended,
                               float z_near) {
  // Calculate boundary of Z near plane in view space.
  float z_near_left = -z_near * std::tan(base::DegToRad(fov_recommended.left));
  float z_near_right = z_near * std::tan(base::DegToRad(fov_recommended.right));
  float z_near_bottom =
      -z_near * std::tan(base::DegToRad(fov_recommended.bottom));
  float z_near_top = z_near * std::tan(base::DegToRad(fov_recommended.top));

  float left = z_near_right;
  float right = z_near_left;
  float bottom = z_near_top;
  float top = z_near_bottom;

  bool has_visible_element = false;

  for (const auto* element : elements) {
    gfx::Transform transform = element->world_space_transform();
    transform.PostConcat(view_matrix);

    // Transform to view space.
    gfx::Point3F left_bottom = transform.MapPoint(gfx::Point3F(-0.5, -0.5, 0));
    gfx::Point3F left_top = transform.MapPoint(gfx::Point3F(-0.5, 0.5, 0));
    gfx::Point3F right_bottom = transform.MapPoint(gfx::Point3F(0.5, -0.5, 0));
    gfx::Point3F right_top = transform.MapPoint(gfx::Point3F(0.5, 0.5, 0));

    // Project point to Z near plane in view space.
    left_bottom.Scale(-z_near / left_bottom.z());
    left_top.Scale(-z_near / left_top.z());
    right_bottom.Scale(-z_near / right_bottom.z());
    right_top.Scale(-z_near / right_top.z());

    // Find bounding box on z near plane.
    float bounds_left = std::min(
        {left_bottom.x(), left_top.x(), right_bottom.x(), right_top.x()});
    float bounds_right = std::max(
        {left_bottom.x(), left_top.x(), right_bottom.x(), right_top.x()});
    float bounds_bottom = std::min(
        {left_bottom.y(), left_top.y(), right_bottom.y(), right_top.y()});
    float bounds_top = std::max(
        {left_bottom.y(), left_top.y(), right_bottom.y(), right_top.y()});

    // Ignore non visible elements.
    if (bounds_left >= z_near_right || bounds_right <= z_near_left ||
        bounds_bottom >= z_near_top || bounds_top <= z_near_bottom ||
        bounds_left == bounds_right || bounds_bottom == bounds_top) {
      continue;
    }

    // Clamp to Z near plane's boundary.
    bounds_left = std::clamp(bounds_left, z_near_left, z_near_right);
    bounds_right = std::clamp(bounds_right, z_near_left, z_near_right);
    bounds_bottom = std::clamp(bounds_bottom, z_near_bottom, z_near_top);
    bounds_top = std::clamp(bounds_top, z_near_bottom, z_near_top);

    left = std::min(bounds_left, left);
    right = std::max(bounds_right, right);
    bottom = std::min(bounds_bottom, bottom);
    top = std::max(bounds_top, top);
    has_visible_element = true;
  }

  if (!has_visible_element) {
    return FovRectangle{0.f, 0.f, 0.f, 0.f};
  }

  // Add a small margin to fix occasional border clipping due to precision.
  const float margin = std::tan(kMargin) * z_near;
  left = std::max(left - margin, z_near_left);
  right = std::min(right + margin, z_near_right);
  bottom = std::max(bottom - margin, z_near_bottom);
  top = std::min(top + margin, z_near_top);

  float left_degrees = base::RadToDeg(std::atan(-left / z_near));
  float right_degrees = base::RadToDeg(std::atan(right / z_near));
  float bottom_degrees = base::RadToDeg(std::atan(-bottom / z_near));
  float top_degrees = base::RadToDeg(std::atan(top / z_near));
  return FovRectangle{left_degrees, right_degrees, bottom_degrees, top_degrees};
}

}  // namespace vr
