// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_openvr_device_hook_base.h"
#include "content/public/common/service_manager_connection.h"
#include "device/vr/openvr/test/test_hook.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

// TODO(https://crbug.com/891832): Remove these conversion functions as part of
// the switch to only mojom types.
device_test::mojom::ControllerRole DeviceToMojoControllerRole(
    device::ControllerRole role) {
  switch (role) {
    case device::kControllerRoleInvalid:
      return device_test::mojom::ControllerRole::kControllerRoleInvalid;
    case device::kControllerRoleRight:
      return device_test::mojom::ControllerRole::kControllerRoleRight;
    case device::kControllerRoleLeft:
      return device_test::mojom::ControllerRole::kControllerRoleLeft;
  }
}

device_test::mojom::ControllerFrameDataPtr DeviceToMojoControllerFrameData(
    const device::ControllerFrameData& data) {
  device_test::mojom::ControllerFrameDataPtr ret =
      device_test::mojom::ControllerFrameData::New();
  ret->packet_number = data.packet_number;
  ret->buttons_pressed = data.buttons_pressed;
  ret->buttons_touched = data.buttons_touched;
  ret->supported_buttons = data.supported_buttons;
  for (unsigned int i = 0; i < device::kMaxNumAxes; ++i) {
    ret->axis_data.emplace_back(device_test::mojom::ControllerAxisData::New());
    ret->axis_data[i]->x = data.axis_data[i].x;
    ret->axis_data[i]->y = data.axis_data[i].y;
    ret->axis_data[i]->axis_type = data.axis_data[i].axis_type;
  }
  ret->role = DeviceToMojoControllerRole(data.role);
  ret->is_valid = data.is_valid;
  ret->pose_data = device_test::mojom::PoseFrameData::New();
  ret->pose_data->device_to_origin = gfx::Transform();
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      ret->pose_data->device_to_origin->matrix().set(
          row, col, data.pose_data.device_to_origin[row * 4 + col]);
    }
  }
  return ret;
}

MockOpenVRDeviceHookBase::MockOpenVRDeviceHookBase()
    : tracked_classes_{device_test::mojom::TrackedDeviceClass::
                           kTrackedDeviceInvalid},
      binding_(this) {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(
      device::mojom::kVrIsolatedServiceName,
      mojo::MakeRequest(&test_hook_registration_));

  device_test::mojom::XRTestHookPtr client;
  binding_.Bind(mojo::MakeRequest(&client));

  mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
  // For now, always have the HMD connected.
  tracked_classes_[0] =
      device_test::mojom::TrackedDeviceClass::kTrackedDeviceHmd;
  test_hook_registration_->SetTestHook(std::move(client));
}

MockOpenVRDeviceHookBase::~MockOpenVRDeviceHookBase() {
  StopHooking();
}

void MockOpenVRDeviceHookBase::StopHooking() {
  // We don't call test_hook_registration_->SetTestHook(nullptr), since that
  // will potentially deadlock with reentrant or crossing synchronous mojo
  // calls.
  binding_.Close();
  test_hook_registration_ = nullptr;
}

void MockOpenVRDeviceHookBase::OnFrameSubmitted(
    device_test::mojom::SubmittedFrameDataPtr frame_data,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  std::move(callback).Run();
}

void MockOpenVRDeviceHookBase::WaitGetDeviceConfig(
    device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback) {
  device_test::mojom::DeviceConfigPtr ret =
      device_test::mojom::DeviceConfig::New();
  ret->interpupillary_distance = 0.1f;
  ret->projection_left = device_test::mojom::ProjectionRaw::New(1, 1, 1, 1);
  ret->projection_right = device_test::mojom::ProjectionRaw::New(1, 1, 1, 1);
  std::move(callback).Run(std::move(ret));
}

void MockOpenVRDeviceHookBase::WaitGetPresentingPose(
    device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback) {
  auto pose = device_test::mojom::PoseFrameData::New();
  pose->device_to_origin = gfx::Transform();
  std::move(callback).Run(std::move(pose));
}

void MockOpenVRDeviceHookBase::WaitGetMagicWindowPose(
    device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback) {
  auto pose = device_test::mojom::PoseFrameData::New();
  pose->device_to_origin = gfx::Transform();
  std::move(callback).Run(std::move(pose));
}

void MockOpenVRDeviceHookBase::WaitGetControllerRoleForTrackedDeviceIndex(
    unsigned int index,
    device_test::mojom::XRTestHook::
        WaitGetControllerRoleForTrackedDeviceIndexCallback callback) {
  auto iter = controller_data_map_.find(index);
  auto role = iter == controller_data_map_.end()
                  ? device_test::mojom::ControllerRole::kControllerRoleInvalid
                  : DeviceToMojoControllerRole(iter->second.role);
  std::move(callback).Run(role);
}

void MockOpenVRDeviceHookBase::WaitGetTrackedDeviceClass(
    unsigned int index,
    device_test::mojom::XRTestHook::WaitGetTrackedDeviceClassCallback
        callback) {
  DCHECK(index < device::kMaxTrackedDevices);
  std::move(callback).Run(tracked_classes_[index]);
}

void MockOpenVRDeviceHookBase::WaitGetControllerData(
    unsigned int index,
    device_test::mojom::XRTestHook::WaitGetControllerDataCallback callback) {
  if (tracked_classes_[index] ==
      device_test::mojom::TrackedDeviceClass::kTrackedDeviceController) {
    auto iter = controller_data_map_.find(index);
    DCHECK(iter != controller_data_map_.end());
    std::move(callback).Run(DeviceToMojoControllerFrameData(iter->second));
    return;
  }
  // Default to not being valid so that controllers aren't connected unless
  // a test specifically enables it.
  auto data =
      CreateValidController(device::ControllerRole::kControllerRoleInvalid);
  data.is_valid = false;
  std::move(callback).Run(DeviceToMojoControllerFrameData(data));
}

unsigned int MockOpenVRDeviceHookBase::ConnectController(
    const device::ControllerFrameData& initial_data) {
  // Find the first open tracked device slot and fill that.
  for (unsigned int i = 0; i < device::kMaxTrackedDevices; ++i) {
    if (tracked_classes_[i] ==
        device_test::mojom::TrackedDeviceClass::kTrackedDeviceInvalid) {
      tracked_classes_[i] =
          device_test::mojom::TrackedDeviceClass::kTrackedDeviceController;
      controller_data_map_.insert(std::make_pair(i, initial_data));
      return i;
    }
  }
  // We shouldn't be running out of slots during a test.
  NOTREACHED();
  // NOTREACHED should make it unnecessary to return here (as it does elsewhere
  // in the code), but compilation fails if this is not present.
  return device::kMaxTrackedDevices;
}

void MockOpenVRDeviceHookBase::UpdateController(
    unsigned int index,
    const device::ControllerFrameData& updated_data) {
  auto iter = controller_data_map_.find(index);
  DCHECK(iter != controller_data_map_.end());
  iter->second = updated_data;
}

void MockOpenVRDeviceHookBase::DisconnectController(unsigned int index) {
  DCHECK(tracked_classes_[index] ==
         device_test::mojom::TrackedDeviceClass::kTrackedDeviceController);
  auto iter = controller_data_map_.find(index);
  DCHECK(iter != controller_data_map_.end());
  controller_data_map_.erase(iter);
  tracked_classes_[index] =
      device_test::mojom::TrackedDeviceClass::kTrackedDeviceInvalid;
}

device::ControllerFrameData MockOpenVRDeviceHookBase::CreateValidController(
    device::ControllerRole role) {
  device::ControllerFrameData ret;
  // Because why shouldn't a 64 button controller exist?
  ret.supported_buttons = UINT64_MAX;
  memset(ret.axis_data, 0,
         sizeof(device::ControllerAxisData) * device::kMaxNumAxes);
  ret.role = role;
  ret.is_valid = true;
  // Identity matrix.
  ret.pose_data.device_to_origin[0] = 1;
  ret.pose_data.device_to_origin[5] = 1;
  ret.pose_data.device_to_origin[10] = 1;
  ret.pose_data.device_to_origin[15] = 1;
  return ret;
}
