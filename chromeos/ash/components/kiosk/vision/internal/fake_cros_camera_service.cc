// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/fake_cros_camera_service.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom-forward.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::kiosk_vision {

FakeCrosCameraService::FakeCrosCameraService()
    : camera_receiver_(this), provider_receiver_(this) {
  ash::mojo_service_manager::GetServiceManagerProxy()->Register(
      chromeos::mojo_services::kCrosCameraService,
      provider_receiver_.BindNewPipeAndPassRemote());
}

FakeCrosCameraService::~FakeCrosCameraService() = default;

bool FakeCrosCameraService::HasObserver() {
  return camera_receiver_.is_bound();
}

void FakeCrosCameraService::EmitFakeDetection(
    cros::mojom::KioskVisionDetectionPtr detection) {
  observer_remote_->OnFrameProcessed(std::move(detection));
}

void FakeCrosCameraService::EmitFakeTrack(
    cros::mojom::KioskVisionTrackPtr track) {
  observer_remote_->OnTrackCompleted(std::move(track));
}

void FakeCrosCameraService::EmitFakeError(cros::mojom::KioskVisionError error) {
  observer_remote_->OnError(error);
}

void FakeCrosCameraService::StartKioskVisionDetection(
    const std::string& dlc_path,
    mojo::PendingRemote<cros::mojom::KioskVisionObserver> observer) {
  observer_remote_.Bind(std::move(observer));
  observer_remote_.reset_on_disconnect();
  bound_future_.SetValue();
}

void FakeCrosCameraService::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr client_identity,
    mojo::ScopedMessagePipeHandle handle) {
  CHECK(handle->is_valid());
  CHECK(!camera_receiver_.is_bound());
  camera_receiver_.Bind(
      mojo::PendingReceiver<cros::mojom::CrosCameraService>(std::move(handle)));
}

}  // namespace ash::kiosk_vision
