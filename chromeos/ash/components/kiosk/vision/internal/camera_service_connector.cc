// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/camera_service_connector.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::kiosk_vision {

namespace {

mojo::Remote<cros::mojom::CrosCameraService> BindCameraServiceRemote() {
  mojo::Remote<cros::mojom::CrosCameraService> remote;
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosCameraService, /*timeout=*/std::nullopt,
      remote.BindNewPipeAndPassReceiver().PassPipe());
  return remote;
}

}  // namespace

CameraServiceConnector::CameraServiceConnector(
    const std::string& dlc_path,
    cros::mojom::KioskVisionObserver* observer)
    : receiver_(observer), camera_service_(BindCameraServiceRemote()) {
  // TODO(b/335458462) Camera service runs in a separate process and a
  // disconnect may happen when it crashes. Implement a reconnect strategy.
  camera_service_.reset_on_disconnect();
  camera_service_->StartKioskVisionDetection(
      dlc_path, receiver_.BindNewPipeAndPassRemote());
}

CameraServiceConnector::~CameraServiceConnector() = default;

}  // namespace ash::kiosk_vision
