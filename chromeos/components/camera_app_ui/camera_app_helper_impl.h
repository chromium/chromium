// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_

#include <vector>

#include "ash/public/cpp/screen_backlight.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/macros.h"
#include "chromeos/components/camera_app_ui/camera_app_helper.mojom.h"
#include "chromeos/components/camera_app_ui/camera_app_ui.h"
#include "chromeos/components/camera_app_ui/camera_app_window_state_controller.h"
#include "chromeos/components/camera_app_ui/document_scanner_service_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace chromeos_camera {

class CameraAppHelperImpl : public ash::TabletModeObserver,
                            public ash::ScreenBacklightObserver,
                            public display::DisplayObserver,
                            public mojom::CameraAppHelper {
 public:
  using CameraResultCallback =
      base::RepeatingCallback<void(uint32_t,
                                   arc::mojom::CameraIntentAction,
                                   const std::vector<uint8_t>&,
                                   HandleCameraResultCallback)>;
  using SendBroadcastCallback =
      base::RepeatingCallback<void(bool, std::string)>;
  using TabletModeMonitor = mojom::TabletModeMonitor;
  using ScreenStateMonitor = mojom::ScreenStateMonitor;
  using ExternalScreenMonitor = mojom::ExternalScreenMonitor;
  using CameraUsageOwnershipMonitor = mojom::CameraUsageOwnershipMonitor;

  CameraAppHelperImpl(chromeos::CameraAppUI* camera_app_ui,
                      CameraResultCallback camera_result_callback,
                      SendBroadcastCallback send_broadcast_callback,
                      aura::Window* window);

  CameraAppHelperImpl(const CameraAppHelperImpl&) = delete;
  CameraAppHelperImpl& operator=(const CameraAppHelperImpl&) = delete;

  ~CameraAppHelperImpl() override;
  void Bind(mojo::PendingReceiver<mojom::CameraAppHelper> receiver);

  // mojom::CameraAppHelper implementations.
  void HandleCameraResult(uint32_t intent_id,
                          arc::mojom::CameraIntentAction action,
                          const std::vector<uint8_t>& data,
                          HandleCameraResultCallback callback) override;
  void IsTabletMode(IsTabletModeCallback callback) override;
  void StartPerfEventTrace(const std::string& event) override;
  void StopPerfEventTrace(const std::string& event) override;
  void SetTabletMonitor(mojo::PendingRemote<TabletModeMonitor> monitor,
                        SetTabletMonitorCallback callback) override;
  void SetScreenStateMonitor(mojo::PendingRemote<ScreenStateMonitor> monitor,
                             SetScreenStateMonitorCallback callback) override;
  void IsMetricsAndCrashReportingEnabled(
      IsMetricsAndCrashReportingEnabledCallback callback) override;
  void SetExternalScreenMonitor(
      mojo::PendingRemote<ExternalScreenMonitor> monitor,
      SetExternalScreenMonitorCallback callback) override;
  void OpenFileInGallery(const std::string& name) override;
  void OpenFeedbackDialog(const std::string& placeholder) override;
  void SetCameraUsageMonitor(
      mojo::PendingRemote<CameraUsageOwnershipMonitor> usage_monitor,
      SetCameraUsageMonitorCallback callback) override;
  void GetWindowStateController(
      GetWindowStateControllerCallback callback) override;
  void SendNewCaptureBroadcast(bool is_video, const std::string& name) override;
  void MonitorFileDeletion(const std::string& name,
                           MonitorFileDeletionCallback callback) override;
  void IsDocumentModeSupported(
      IsDocumentModeSupportedCallback callback) override;
  void ScanDocumentCorners(const std::vector<uint8_t>& jpeg_data,
                           ScanDocumentCornersCallback callback) override;
  void ConvertToDocument(
      const std::vector<uint8_t>& jpeg_data,
      const std::vector<gfx::PointF>& corners,
      chromeos_camera::mojom::DocumentOutputFormat output_format,
      ConvertToDocumentCallback callback) override;
  void ConvertToPdf(const std::vector<uint8_t>& jpeg_data,
                    ConvertToPdfCallback callback) override;

 private:
  void CheckExternalScreenState();

  void OnScannedDocumentCorners(ScanDocumentCornersCallback callback,
                                bool success,
                                const std::vector<gfx::PointF>& corners);
  void OnConvertedToDocument(
      chromeos_camera::mojom::DocumentOutputFormat output_format,
      ConvertToDocumentCallback callback,
      bool success,
      const std::vector<uint8_t>& processed_jpeg_data);

  // ash::TabletModeObserver overrides;
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ash::ScreenBacklightObserver overrides;
  void OnScreenBacklightStateChanged(
      ash::ScreenBacklightState screen_backlight_state) override;

  // display::DisplayObserver overrides;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  // For platform app, we set |camera_app_ui_| to nullptr and should not use
  // it. For SWA, since CameraAppUI owns CameraAppHelperImpl, it is safe to
  // assume that the |camera_app_ui_| is always valid during the whole lifetime
  // of CameraAppHelperImpl.
  chromeos::CameraAppUI* camera_app_ui_;

  CameraResultCallback camera_result_callback_;

  SendBroadcastCallback send_broadcast_callback_;

  bool has_external_screen_;

  absl::optional<uint32_t> pending_intent_id_;

  aura::Window* window_;

  mojo::Remote<TabletModeMonitor> tablet_mode_monitor_;
  mojo::Remote<ScreenStateMonitor> screen_state_monitor_;
  mojo::Remote<ExternalScreenMonitor> external_screen_monitor_;

  mojo::Receiver<chromeos_camera::mojom::CameraAppHelper> receiver_{this};

  std::unique_ptr<chromeos::CameraAppWindowStateController>
      window_state_controller_;

  display::ScopedDisplayObserver display_observer_{this};

  // Client to connect to document detection service.
  std::unique_ptr<chromeos::DocumentScannerServiceClient>
      document_scanner_service_;
};

}  // namespace chromeos_camera

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
