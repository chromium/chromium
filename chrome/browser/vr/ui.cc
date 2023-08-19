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
#include "base/numerics/math_constants.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/skia_surface_provider_factory.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

namespace {

constexpr float kMargin = 1.f * base::kPiFloat / 180;

UiElementName UserFriendlyElementNameToUiElementName(
    UserFriendlyElementName name) {
  switch (name) {
    case UserFriendlyElementName::kUrl:
      return kUrlBarOriginRegion;
    case UserFriendlyElementName::kBackButton:
      return kUrlBarBackButton;
    case UserFriendlyElementName::kForwardButton:
      return kOverflowMenuForwardButton;
    case UserFriendlyElementName::kReloadButton:
      return kOverflowMenuReloadButton;
    case UserFriendlyElementName::kOverflowMenu:
      return kUrlBarOverflowButton;
    case UserFriendlyElementName::kPageInfoButton:
      return kUrlBarSecurityButton;
    case UserFriendlyElementName::kBrowsingDialog:
      return k2dBrowsingHostedUiContent;
    case UserFriendlyElementName::kContentQuad:
      return kContentQuad;
    case UserFriendlyElementName::kNewIncognitoTab:
      return kOverflowMenuNewIncognitoTabItem;
    case UserFriendlyElementName::kCloseIncognitoTabs:
      return kOverflowMenuCloseAllIncognitoTabsItem;
    case UserFriendlyElementName::kExitPrompt:
      return kExitPrompt;
    case UserFriendlyElementName::kSuggestionBox:
      return kOmniboxSuggestions;
    case UserFriendlyElementName::kOmniboxTextField:
      return kOmniboxTextField;
    case UserFriendlyElementName::kOmniboxCloseButton:
      return kOmniboxCloseButton;
    case UserFriendlyElementName::kOmniboxVoiceInputButton:
      return kOmniboxVoiceSearchButton;
    case UserFriendlyElementName::kVoiceInputCloseButton:
      return kSpeechRecognitionListeningCloseButton;
    case UserFriendlyElementName::kAppButtonExitToast:
      return kWebVrExclusiveScreenToast;
    case UserFriendlyElementName::kWebXrAudioIndicator:
      return kWebVrAudioCaptureIndicator;
    case UserFriendlyElementName::kWebXrHostedContent:
      return kWebVrHostedUiContent;
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
      NOTREACHED();
      return kNone;
  }
}

}  // namespace

Ui::Ui(UiBrowserInterface* browser, const UiInitialState& ui_initial_state)
    : browser_(browser),
      scene_(std::make_unique<UiScene>()),
      model_(std::make_unique<Model>()) {
  UiInitialState state = ui_initial_state;
  InitializeModel(state);

  UiSceneCreator(browser, scene_.get(), this, model_.get()).CreateScene();
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

void Ui::OnPause() {}

void Ui::OnMenuButtonClicked() {
  if (!model_->gvr_input_support) {
    return;
  }

  // Menu button click exits the WebVR presentation and fullscreen.
  browser_->ExitPresent();
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

void Ui::Dump(bool include_bindings) {
#ifndef NDEBUG
  std::ostringstream os;
  os << std::setprecision(3);
  os << std::endl;
  scene_->root_element().DumpHierarchy(std::vector<size_t>(), &os,
                                       include_bindings);

  std::stringstream ss(os.str());
  std::string line;
  while (std::getline(ss, line, '\n')) {
    LOG(ERROR) << line;
  }
#endif
}

void Ui::ReinitializeForTest(const UiInitialState& ui_initial_state) {
  InitializeModel(ui_initial_state);
}

bool Ui::GetElementVisibilityForTesting(UserFriendlyElementName element_name) {
  auto* target_element = scene()->GetUiElementByName(
      UserFriendlyElementNameToUiElementName(element_name));
  DCHECK(target_element) << "Unsupported test element";
  return target_element->IsVisible();
}

void Ui::InitializeModel(const UiInitialState& ui_initial_state) {
  model_->web_vr.has_received_permissions = false;
  model_->web_vr.state = kWebVrAwaitingFirstFrame;

  model_->gvr_input_support = ui_initial_state.gvr_input_support;
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

void Ui::DrawWebXr(int texture_data_handle, const float (&uv_transform)[16]) {
  if (!texture_data_handle)
    return;
  ui_element_renderer_->DrawTextureCopy(texture_data_handle, uv_transform, 0,
                                        0);
}

void Ui::DrawWebVrOverlayForeground(const vr::RenderInfo& info) {
  ui_renderer_->DrawWebVrOverlayForeground(info);
}

bool Ui::HasWebXrOverlayElementsToDraw() {
  return scene_->HasWebXrOverlayElementsToDraw();
}

void Ui::HandleMenuButtonEvents(InputEventList* input_event_list) {
  auto it = input_event_list->begin();
  while (it != input_event_list->end()) {
    if (InputEvent::IsMenuButtonEventType((*it)->type())) {
      switch ((*it)->type()) {
        case InputEvent::kMenuButtonClicked:
          // Post a task, rather than calling directly, to avoid modifying UI
          // state in the midst of frame rendering.
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&Ui::OnMenuButtonClicked, base::Unretained(this)));
          break;
        case InputEvent::kMenuButtonLongPressStart:
          model_->menu_button_long_pressed = true;
          break;
        case InputEvent::kMenuButtonLongPressEnd:
          model_->menu_button_long_pressed = false;
          break;
        default:
          NOTREACHED();
      }
      it = input_event_list->erase(it);
    } else {
      ++it;
    }
  }
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
  float z_near_left =
      -z_near * std::tan(fov_recommended.left * base::kPiFloat / 180);
  float z_near_right =
      z_near * std::tan(fov_recommended.right * base::kPiFloat / 180);
  float z_near_bottom =
      -z_near * std::tan(fov_recommended.bottom * base::kPiFloat / 180);
  float z_near_top =
      z_near * std::tan(fov_recommended.top * base::kPiFloat / 180);

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

  float left_degrees = std::atan(-left / z_near) * 180 / base::kPiFloat;
  float right_degrees = std::atan(right / z_near) * 180 / base::kPiFloat;
  float bottom_degrees = std::atan(-bottom / z_near) * 180 / base::kPiFloat;
  float top_degrees = std::atan(top / z_near) * 180 / base::kPiFloat;
  return FovRectangle{left_degrees, right_degrees, bottom_degrees, top_degrees};
}

#if BUILDFLAG(IS_ANDROID)
extern "C" {
// This symbol is retrieved from the VR feature module library via dlsym(),
// where it's bare address is type-cast to a CreateUiFunction pointer and
// executed. The forward declaration here ensures that the signatures match.
CreateUiFunction CreateUi;
__attribute__((visibility("default"))) UiInterface* CreateUi(
    UiBrowserInterface* browser,
    const UiInitialState& ui_initial_state) {
  return new Ui(browser, ui_initial_state);
}
}  // extern "C"
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace vr
