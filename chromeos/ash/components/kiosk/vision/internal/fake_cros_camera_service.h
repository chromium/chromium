// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_FAKE_CROS_CAMERA_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_FAKE_CROS_CAMERA_SERVICE_H_

#include <string>

#include "base/test/test_future.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace ash::kiosk_vision {

// Fake implementation of `CrosCameraService` for `KioskVision` tests.
//
// Upon construction it creates a `FakeMojoServiceManager`, and registers itself
// under the `chromeos::mojo_services::kCrosCameraService` service name. This
// makes it available to `KioskVision` via the service manager.
//
// Upon deletion, `FakeMojoServiceManager` gets destroyed and cleans up the
// registrations from this class.
class FakeCrosCameraService
    : public cros::mojom::CrosCameraService,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  FakeCrosCameraService();
  FakeCrosCameraService(const FakeCrosCameraService&) = delete;
  FakeCrosCameraService& operator=(const FakeCrosCameraService&) = delete;
  ~FakeCrosCameraService() override;

  // Returns true if `observer_remote_` is currently bound.
  //
  // `observer_remote_` is bound after a call to `StartKioskVisionDetection`.
  bool HasObserver();

  // Waits until the observer is ready. See `HasObserver()`.
  [[nodiscard]] bool WaitForObserver() { return bound_future_.Wait(); }

  // Emits a fake detection event to `observer_remote_`. An observer must have
  // been previously bound with `StartKioskVisionDetection`.
  void EmitFakeDetection(cros::mojom::KioskVisionDetectionPtr detection);

  // Emits a fake track completed event to `observer_remote_`. An observer must
  // have been previously bound with `StartKioskVisionDetection`.
  void EmitFakeTrack(cros::mojom::KioskVisionTrackPtr track);

  // Emits a fake error event to `observer_remote_`. An observer must have been
  // previously bound with `StartKioskVisionDetection`.
  void EmitFakeError(cros::mojom::KioskVisionError error);

 private:
  // `cros::mojom::CrosCameraService` implementation of relevant functions.
  void StartKioskVisionDetection(
      const std::string& dlc_path,
      ::mojo::PendingRemote<cros::mojom::KioskVisionObserver> observer)
      override;

  // `cros::mojom::CrosCameraService` implementation of unused functions.
  void GetCameraModule(cros::mojom::CameraClientType type,
                       GetCameraModuleCallback callback) override {}
  void SetTracingEnabled(bool enabled) override {}
  void SetAutoFramingState(cros::mojom::CameraAutoFramingState state) override {
  }
  void GetCameraSWPrivacySwitchState(
      GetCameraSWPrivacySwitchStateCallback callback) override {}
  void SetCameraSWPrivacySwitchState(
      cros::mojom::CameraPrivacySwitchState state) override {}
  void GetAutoFramingSupported(
      GetAutoFramingSupportedCallback callback) override {}
  void SetCameraEffect(::cros::mojom::EffectsConfigPtr config,
                       SetCameraEffectCallback callback) override {}
  void AddCrosCameraServiceObserver(
      ::mojo::PendingRemote<cros::mojom::CrosCameraServiceObserver> observer)
      override {}

  // `chromeos::mojo_service_manager::mojom::ServiceProvider` implementation.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr client_identity,
      mojo::ScopedMessagePipeHandle handle) override;

  mojo_service_manager::FakeMojoServiceManager fake_mojo_service_manager_;

  mojo::Remote<cros::mojom::KioskVisionObserver> observer_remote_;
  mojo::Receiver<cros::mojom::CrosCameraService> camera_receiver_;
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_;

  base::test::TestFuture<void> bound_future_;
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_FAKE_CROS_CAMERA_SERVICE_H_
