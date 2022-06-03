// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/input_delegate_for_testing.h"

#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/platform_controller_for_testing.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/quaternion.h"

namespace {

// Laser origin relative to the center of the controller.
constexpr gfx::Point3F kLaserOriginOffset = {0.0f, 0.0f, -0.05f};
constexpr gfx::Vector3dF kForwardVector(0.0f, 0.0f, -1.0f);
constexpr gfx::Vector3dF kStartControllerPosition(0.3, -0.3, -0.3);

// We position the controller in a fixed position (no arm model).
// The location constants are approximations that allow us to have the
// controller and the laser visible on the screenshots.
void SetOriginAndTransform(vr::ControllerModel* model) {
  gfx::Transform mat;
  mat.Translate3d(kStartControllerPosition);
  mat.PreconcatTransform(
      gfx::Transform(gfx::Quaternion(kForwardVector, model->laser_direction)));
  model->transform = mat;
  model->laser_origin = kLaserOriginOffset;
  mat.TransformPoint(&model->laser_origin);
}

}  // namespace

namespace vr {

InputDelegateForTesting::InputDelegateForTesting(UiInterface* ui)
    : ui_(ui), last_touchpad_timestamp_(base::TimeTicks()) {
  gesture_detector_ = std::make_unique<GestureDetector>();
  cached_controller_model_.laser_direction = kForwardVector;
  SetOriginAndTransform(&cached_controller_model_);
}

InputDelegateForTesting::~InputDelegateForTesting() = default;

gfx::Transform InputDelegateForTesting::GetHeadPose() {
  return gfx::Transform();
}

void InputDelegateForTesting::OnTriggerEvent(bool pressed) {
  NOTREACHED();
}

void InputDelegateForTesting::QueueControllerActionForTesting(
    ControllerTestInput controller_input) {
  DCHECK_NE(controller_input.action,
            VrControllerTestAction::kRevertToRealInput);
  ControllerModel controller_model;
  if (controller_input.element_name == UserFriendlyElementName::kNone) {
    controller_model.laser_direction = kForwardVector;
    SetOriginAndTransform(&controller_model);
  } else if (controller_input.element_name ==
             UserFriendlyElementName::kCurrentPosition) {
    controller_model.transform = GetMostRecentModel().transform;
    controller_model.laser_direction = GetMostRecentModel().laser_direction;
    controller_model.laser_origin = GetMostRecentModel().laser_origin;
  } else {
    auto target_point = ui_->GetTargetPointForTesting(
        controller_input.element_name, controller_input.position);
    auto direction = (target_point - kStartControllerPosition) - kOrigin;
    direction.GetNormalized(&controller_model.laser_direction);
    SetOriginAndTransform(&controller_model);
  }

  switch (controller_input.action) {
    case VrControllerTestAction::kHover:
      FALLTHROUGH;
    case VrControllerTestAction::kClickUp:
      controller_model.touchpad_button_state =
          ControllerModel::ButtonState::kUp;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kClickDown:
      controller_model.touchpad_button_state =
          ControllerModel::ButtonState::kDown;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kMove:
      // Use whatever the last button state is.
      controller_model.touchpad_button_state =
          GetMostRecentModel().touchpad_button_state;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kAppDown:
      controller_model.app_button_state = ControllerModel::ButtonState::kDown;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kAppUp:
      controller_model.app_button_state = ControllerModel::ButtonState::kUp;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kTouchDown:
      // Use whatever the most recent direction is and interpret the provided
      // point as the point on the touchpad. Also keep whatever the last
      // touchpad button state was.
      controller_model.laser_direction = GetMostRecentModel().laser_direction;
      SetOriginAndTransform(&controller_model);
      controller_model.touching_touchpad = true;
      controller_model.touchpad_touch_position = controller_input.position;
      controller_model.touchpad_button_state =
          GetMostRecentModel().touchpad_button_state;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kTouchUp:
      controller_model.laser_direction = GetMostRecentModel().laser_direction;
      SetOriginAndTransform(&controller_model);
      controller_model.touching_touchpad = false;
      controller_model.touchpad_touch_position = controller_input.position;
      controller_model.touchpad_button_state =
          GetMostRecentModel().touchpad_button_state;
      controller_model_queue_.push(controller_model);
      break;
    default:
      NOTREACHED() << "Given unsupported controller action";
  }
}

bool InputDelegateForTesting::IsQueueEmpty() const {
  return controller_model_queue_.empty();
}

void InputDelegateForTesting::UpdateController(const gfx::Transform& head_pose,
                                               base::TimeTicks current_time,
                                               bool is_webxr_frame) {
  previous_controller_model_ = cached_controller_model_;
  if (!controller_model_queue_.empty()) {
    cached_controller_model_ = controller_model_queue_.front();
    controller_model_queue_.pop();
  }
  cached_controller_model_.last_orientation_timestamp = current_time;
  cached_controller_model_.last_button_timestamp = current_time;
  if (cached_controller_model_.touching_touchpad) {
    last_touchpad_timestamp_ = current_time;
  }
}

ControllerModel InputDelegateForTesting::GetControllerModel(
    const gfx::Transform& head_pose) {
  return cached_controller_model_;
}

InputEventList InputDelegateForTesting::GetGestures(
    base::TimeTicks current_time) {
  PlatformControllerForTesting controller(&previous_controller_model_,
                                          &cached_controller_model_,
                                          last_touchpad_timestamp_);
  return gesture_detector_->DetectGestures(controller, current_time);
}

device::mojom::XRInputSourceStatePtr
InputDelegateForTesting::GetInputSourceState() {
  auto state = device::mojom::XRInputSourceState::New();
  state->description = device::mojom::XRInputSourceDescription::New();
  state->source_id = 1;
  state->description->target_ray_mode =
      device::mojom::XRTargetRayMode::POINTING;
  state->emulated_position = true;

  return state;
}

void InputDelegateForTesting::OnResume() {}

void InputDelegateForTesting::OnPause() {}

ControllerModel InputDelegateForTesting::GetMostRecentModel() {
  if (!IsQueueEmpty())
    return controller_model_queue_.back();
  return cached_controller_model_;
}

}  // namespace vr
