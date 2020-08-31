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
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace aura {
class Window;
}  // namespace aura

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
  using TabletModeMonitor = mojom::TabletModeMonitor;
  using ScreenStateMonitor = mojom::ScreenStateMonitor;
  using ExternalScreenMonitor = mojom::ExternalScreenMonitor;

  CameraAppHelperImpl(chromeos::CameraAppUI* camera_app_ui,
                      CameraResultCallback camera_result_callback,
                      aura::Window* window);
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

 private:
  void CheckExternalScreenState();

  // ash::TabletModeObserver overrides;
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ash::ScreenBacklightObserver overrides;
  void OnScreenStateChanged(ash::ScreenState screen_state) override;

  // display::DisplayObserver overrides;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  // For platform app, we set |camera_app_ui_| to nullptr and should not use
  // it. For SWA, since CameraAppUI owns CameraAppHelperImpl, it is safe to
  // assume that the |camera_app_ui_| is always valid during the whole lifetime
  // of CameraAppHelperImpl.
  chromeos::CameraAppUI* camera_app_ui_;

  CameraResultCallback camera_result_callback_;

  bool has_external_screen_;

  mojo::Remote<TabletModeMonitor> tablet_monitor_;
  mojo::Remote<ScreenStateMonitor> screen_state_monitor_;
  mojo::Remote<ExternalScreenMonitor> external_screen_monitor_;

  mojo::Receiver<chromeos_camera::mojom::CameraAppHelper> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(CameraAppHelperImpl);
};

}  // namespace chromeos_camera

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
