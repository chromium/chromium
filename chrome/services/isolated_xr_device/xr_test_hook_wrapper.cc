// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_test_hook_wrapper.h"

namespace device {

// TODO(https://crbug.com/891832): Remove these as conversion functions as part
// of the switch to only mojom types.
ControllerRole MojoToDeviceControllerRole(
    device_test::mojom::ControllerRole role) {
  switch (role) {
    case device_test::mojom::ControllerRole::kControllerRoleInvalid:
      return device::kControllerRoleInvalid;
    case device_test::mojom::ControllerRole::kControllerRoleLeft:
      return device::kControllerRoleLeft;
    case device_test::mojom::ControllerRole::kControllerRoleRight:
      return device::kControllerRoleRight;
  }
  return device::kControllerRoleInvalid;
}

PoseFrameData MojoToDevicePoseFrameData(
    device_test::mojom::PoseFrameDataPtr& pose) {
  PoseFrameData ret = {};
  ret.is_valid = !!pose->device_to_origin;
  if (ret.is_valid) {
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        ret.device_to_origin[row * 4 + col] =
            pose->device_to_origin->matrix().getFloat(row, col);
      }
    }
  }

  return ret;
}

XRTestHookWrapper::XRTestHookWrapper(
    device_test::mojom::XRTestHookPtrInfo hook_info)
    : hook_info_(std::move(hook_info)) {}

void XRTestHookWrapper::OnFrameSubmitted(SubmittedFrameData frame_data) {
  if (hook_) {
    auto submitted = device_test::mojom::SubmittedFrameData::New();
    submitted->color =
        device_test::mojom::Color::New(frame_data.color.r, frame_data.color.g,
                                       frame_data.color.b, frame_data.color.a);

    submitted->image_size =
        gfx::Size(frame_data.image_width, frame_data.image_height);
    submitted->eye = frame_data.left_eye ? device_test::mojom::Eye::LEFT
                                         : device_test::mojom::Eye::RIGHT;
    submitted->viewport =
        gfx::Rect(frame_data.viewport.left, frame_data.viewport.right,
                  frame_data.viewport.top, frame_data.viewport.bottom);
    hook_->OnFrameSubmitted(std::move(submitted));
  }
}

DeviceConfig XRTestHookWrapper::WaitGetDeviceConfig() {
  if (hook_) {
    device_test::mojom::DeviceConfigPtr config;
    hook_->WaitGetDeviceConfig(&config);
    if (config) {
      DeviceConfig ret = {};
      ret.interpupillary_distance = config->interpupillary_distance;
      ret.viewport_left[0] = config->projection_left->left;
      ret.viewport_left[1] = config->projection_left->right;
      ret.viewport_left[2] = config->projection_left->top;
      ret.viewport_left[3] = config->projection_left->left;

      ret.viewport_right[0] = config->projection_right->left;
      ret.viewport_right[1] = config->projection_right->right;
      ret.viewport_right[2] = config->projection_right->top;
      ret.viewport_right[3] = config->projection_right->left;
      return ret;
    }
  }

  return {};
}

PoseFrameData XRTestHookWrapper::WaitGetPresentingPose() {
  if (hook_) {
    device_test::mojom::PoseFrameDataPtr pose;
    hook_->WaitGetPresentingPose(&pose);
    if (pose) {
      return MojoToDevicePoseFrameData(pose);
    }
  }

  return {};
}

PoseFrameData XRTestHookWrapper::WaitGetMagicWindowPose() {
  if (hook_) {
    device_test::mojom::PoseFrameDataPtr pose;
    hook_->WaitGetMagicWindowPose(&pose);
    if (pose) {
      return MojoToDevicePoseFrameData(pose);
    }
  }

  return {};
}

ControllerRole XRTestHookWrapper::WaitGetControllerRoleForTrackedDeviceIndex(
    unsigned int index) {
  if (hook_) {
    device_test::mojom::ControllerRole role;
    hook_->WaitGetControllerRoleForTrackedDeviceIndex(index, &role);
    return MojoToDeviceControllerRole(role);
  }

  return device::kControllerRoleInvalid;
}

TrackedDeviceClass XRTestHookWrapper::WaitGetTrackedDeviceClass(
    unsigned int index) {
  if (hook_) {
    device_test::mojom::TrackedDeviceClass device_class;
    hook_->WaitGetTrackedDeviceClass(index, &device_class);
    switch (device_class) {
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceInvalid:
        return device::kTrackedDeviceInvalid;
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceHmd:
        return device::kTrackedDeviceHmd;
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceController:
        return device::kTrackedDeviceController;
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceGenericTracker:
        return device::kTrackedDeviceGenericTracker;
      case device_test::mojom::TrackedDeviceClass::
          kTrackedDeviceTrackingReference:
        return device::kTrackedDeviceTrackingReference;
      case device_test::mojom::TrackedDeviceClass::
          kTrackedDeviceDisplayRedirect:
        return device::kTrackedDeviceDisplayRedirect;
    }
  }

  return device::kTrackedDeviceInvalid;
}

ControllerFrameData XRTestHookWrapper::WaitGetControllerData(
    unsigned int index) {
  if (hook_) {
    device_test::mojom::ControllerFrameDataPtr data;
    hook_->WaitGetControllerData(index, &data);
    if (data) {
      ControllerFrameData ret = {};
      ret.packet_number = data->packet_number;
      ret.buttons_pressed = data->buttons_pressed;
      ret.buttons_touched = data->buttons_touched;
      ret.supported_buttons = data->supported_buttons;
      ret.pose_data = MojoToDevicePoseFrameData(data->pose_data);
      ret.role = MojoToDeviceControllerRole(data->role);
      ret.is_valid = data->is_valid;
      for (unsigned int i = 0; i < device::kMaxNumAxes; ++i) {
        ret.axis_data[i].x = data->axis_data[i]->x;
        ret.axis_data[i].y = data->axis_data[i]->y;
        ret.axis_data[i].axis_type = data->axis_data[i]->axis_type;
      }
      return ret;
    }
  }

  return {};
}

void XRTestHookWrapper::AttachCurrentThread() {
  if (hook_info_) {
    hook_.Bind(std::move(hook_info_));
  }

  current_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void XRTestHookWrapper::DetachCurrentThread() {
  if (hook_) {
    hook_info_ = hook_.PassInterface();
  }

  current_task_runner_ = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
XRTestHookWrapper::GetBoundTaskRunner() {
  return current_task_runner_;
}

XRTestHookWrapper::~XRTestHookWrapper() = default;

}  // namespace device
