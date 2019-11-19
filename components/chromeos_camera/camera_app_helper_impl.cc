// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/camera_app_helper_impl.h"

#include "ash/public/cpp/tablet_mode.h"

namespace chromeos_camera {

CameraAppHelperImpl::CameraAppHelperImpl(
    CameraResultCallback camera_result_callback)
    : camera_result_callback_(std::move(camera_result_callback)) {}

CameraAppHelperImpl::~CameraAppHelperImpl() = default;

void CameraAppHelperImpl::HandleCameraResult(
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    HandleCameraResultCallback callback) {
  camera_result_callback_.Run(intent_id, action, data, std::move(callback));
}

void CameraAppHelperImpl::IsTabletMode(IsTabletModeCallback callback) {
  std::move(callback).Run(ash::TabletMode::Get()->InTabletMode());
}

}  // namespace chromeos_camera
