// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_CAMERA_SERVICE_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_CAMERA_SERVICE_CONNECTOR_H_

#include <string>

#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::kiosk_vision {

// Binds a remote to `CrosCameraService`, and registers the given `observer` for
// detection events.
class CameraServiceConnector {
 public:
  CameraServiceConnector(const std::string& dlc_path,
                         cros::mojom::KioskVisionObserver* observer);
  CameraServiceConnector(const CameraServiceConnector&) = delete;
  CameraServiceConnector& operator=(const CameraServiceConnector&) = delete;
  ~CameraServiceConnector();

 private:
  mojo::Receiver<cros::mojom::KioskVisionObserver> receiver_;
  mojo::Remote<cros::mojom::CrosCameraService> camera_service_;
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_CAMERA_SERVICE_CONNECTOR_H_
