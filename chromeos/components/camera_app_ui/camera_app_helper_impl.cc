// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/camera_app_ui/camera_app_helper_impl.h"

#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace chromeos_camera {
namespace {

mojom::ScreenState ToMojoScreenState(ash::ScreenState s) {
  switch (s) {
    case ash::ScreenState::ON:
      return mojom::ScreenState::ON;
    case ash::ScreenState::OFF:
      return mojom::ScreenState::OFF;
    case ash::ScreenState::OFF_AUTO:
      return mojom::ScreenState::OFF_AUTO;
    default:
      NOTREACHED();
  }
}

bool HasExternalScreen() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (!display.IsInternal()) {
      return true;
    }
  }
  return false;
}

}  // namespace

CameraAppHelperImpl::CameraAppHelperImpl(
    chromeos::CameraAppUI* camera_app_ui,
    CameraResultCallback camera_result_callback,
    aura::Window* window)
    : camera_app_ui_(camera_app_ui),
      camera_result_callback_(std::move(camera_result_callback)),
      has_external_screen_(HasExternalScreen()),
      window_(window) {
  DCHECK(window);
  window->SetProperty(ash::kCanConsumeSystemKeysKey, true);
  ash::TabletMode::Get()->AddObserver(this);
  ash::ScreenBacklight::Get()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

CameraAppHelperImpl::~CameraAppHelperImpl() {
  ash::TabletMode::Get()->RemoveObserver(this);
  ash::ScreenBacklight::Get()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
}

void CameraAppHelperImpl::Bind(
    mojo::PendingReceiver<mojom::CameraAppHelper> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

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

void CameraAppHelperImpl::StartPerfEventTrace(const std::string& event) {
  TRACE_EVENT_BEGIN0("camera", event.c_str());
}

void CameraAppHelperImpl::StopPerfEventTrace(const std::string& event) {
  TRACE_EVENT_END0("camera", event.c_str());
}

void CameraAppHelperImpl::SetTabletMonitor(
    mojo::PendingRemote<TabletModeMonitor> monitor,
    SetTabletMonitorCallback callback) {
  tablet_mode_monitor_ = mojo::Remote<TabletModeMonitor>(std::move(monitor));
  std::move(callback).Run(ash::TabletMode::Get()->InTabletMode());
}

void CameraAppHelperImpl::SetScreenStateMonitor(
    mojo::PendingRemote<ScreenStateMonitor> monitor,
    SetScreenStateMonitorCallback callback) {
  screen_state_monitor_ = mojo::Remote<ScreenStateMonitor>(std::move(monitor));
  auto&& mojo_state =
      ToMojoScreenState(ash::ScreenBacklight::Get()->GetScreenState());
  std::move(callback).Run(mojo_state);
}

void CameraAppHelperImpl::IsMetricsAndCrashReportingEnabled(
    IsMetricsAndCrashReportingEnabledCallback callback) {
  DCHECK_NE(camera_app_ui_, nullptr);
  std::move(callback).Run(
      camera_app_ui_->delegate()->IsMetricsAndCrashReportingEnabled());
}

void CameraAppHelperImpl::SetExternalScreenMonitor(
    mojo::PendingRemote<ExternalScreenMonitor> monitor,
    SetExternalScreenMonitorCallback callback) {
  external_screen_monitor_ =
      mojo::Remote<ExternalScreenMonitor>(std::move(monitor));
  std::move(callback).Run(has_external_screen_);
}

void CameraAppHelperImpl::CheckExternalScreenState() {
  if (has_external_screen_ == HasExternalScreen())
    return;
  has_external_screen_ = !has_external_screen_;

  if (external_screen_monitor_.is_bound())
    external_screen_monitor_->Update(has_external_screen_);
}

void CameraAppHelperImpl::OpenFileInGallery(const std::string& name) {
  DCHECK_NE(camera_app_ui_, nullptr);
  camera_app_ui_->delegate()->OpenFileInGallery(name);
}

void CameraAppHelperImpl::OpenFeedbackDialog(const std::string& placeholder) {
  DCHECK_NE(camera_app_ui_, nullptr);
  camera_app_ui_->delegate()->OpenFeedbackDialog(placeholder);
}

void CameraAppHelperImpl::SetCameraUsageMonitor(
    mojo::PendingRemote<CameraUsageOwnershipMonitor> usage_monitor,
    SetCameraUsageMonitorCallback callback) {
  DCHECK_NE(camera_app_ui_, nullptr);
  camera_app_ui_->app_window_manager()->SetCameraUsageMonitor(
      window_, std::move(usage_monitor), std::move(callback));
}

void CameraAppHelperImpl::GetWindowStateController(
    GetWindowStateControllerCallback callback) {
  DCHECK_NE(camera_app_ui_, nullptr);

  if (!window_state_controller_) {
    window_state_controller_ =
        std::make_unique<chromeos::CameraAppWindowStateController>(
            views::Widget::GetWidgetForNativeWindow(window_));
  }
  mojo::PendingRemote<chromeos_camera::mojom::WindowStateController>
      controller_remote;
  window_state_controller_->AddReceiver(
      controller_remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(controller_remote));
}

void CameraAppHelperImpl::OnTabletModeStarted() {
  if (tablet_mode_monitor_.is_bound())
    tablet_mode_monitor_->Update(true);
}

void CameraAppHelperImpl::OnTabletModeEnded() {
  if (tablet_mode_monitor_.is_bound())
    tablet_mode_monitor_->Update(false);
}

void CameraAppHelperImpl::OnScreenStateChanged(ash::ScreenState screen_state) {
  if (screen_state_monitor_.is_bound())
    screen_state_monitor_->Update(ToMojoScreenState(screen_state));
}

void CameraAppHelperImpl::OnDisplayAdded(const display::Display& new_display) {
  CheckExternalScreenState();
}

void CameraAppHelperImpl::OnDisplayRemoved(
    const display::Display& old_display) {
  CheckExternalScreenState();
}

}  // namespace chromeos_camera
