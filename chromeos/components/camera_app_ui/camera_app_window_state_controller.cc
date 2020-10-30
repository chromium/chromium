// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/camera_app_ui/camera_app_window_state_controller.h"

namespace chromeos {

CameraAppWindowStateController::CameraAppWindowStateController(
    views::Widget* widget)
    : widget_(widget), window_state_(GetCurrentWindowState()) {
  widget_->AddObserver(this);
}

CameraAppWindowStateController::~CameraAppWindowStateController() {
  widget_->RemoveObserver(this);
}

void CameraAppWindowStateController::AddReceiver(
    mojo::PendingReceiver<chromeos_camera::mojom::WindowStateController>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CameraAppWindowStateController::AddMonitor(
    mojo::PendingRemote<chromeos_camera::mojom::WindowStateMonitor> monitor,
    AddMonitorCallback callback) {
  auto remote = mojo::Remote<chromeos_camera::mojom::WindowStateMonitor>(
      std::move(monitor));
  monitors_.push_back(std::move(remote));
  std::move(callback).Run(window_state_);
}

void CameraAppWindowStateController::GetWindowState(
    GetWindowStateCallback callback) {
  std::move(callback).Run(window_state_);
}

void CameraAppWindowStateController::Minimize(MinimizeCallback callback) {
  minimize_callbacks_.push(std::move(callback));
  widget_->Minimize();
}

void CameraAppWindowStateController::Restore(RestoreCallback callback) {
  restore_callbacks_.push(std::move(callback));
  widget_->Restore();
}

void CameraAppWindowStateController::Maximize(MaximizeCallback callback) {
  maximize_callbacks_.push(std::move(callback));
  widget_->Maximize();
}

void CameraAppWindowStateController::Fullscreen(FullscreenCallback callback) {
  fullscreen_callbacks_.push(std::move(callback));
  widget_->SetFullscreen(true);
}

void CameraAppWindowStateController::Focus(FocusCallback callback) {
  focus_callbacks_.push(std::move(callback));
  widget_->Activate();
}

void CameraAppWindowStateController::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  OnWindowStateChanged();
}

void CameraAppWindowStateController::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  while (!focus_callbacks_.empty()) {
    std::move(focus_callbacks_.front()).Run();
    focus_callbacks_.pop();
  }
}

void CameraAppWindowStateController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  OnWindowStateChanged();
}

void CameraAppWindowStateController::OnWindowStateChanged() {
  auto prev_state = window_state_;
  window_state_ = GetCurrentWindowState();

  std::queue<base::OnceClosure>* callbacks;
  switch (window_state_) {
    case WindowStateType::MINIMIZED:
      callbacks = &minimize_callbacks_;
      break;
    case WindowStateType::REGULAR:
      callbacks = &restore_callbacks_;
      break;
    case WindowStateType::MAXIMIZED:
      callbacks = &maximize_callbacks_;
      break;
    case WindowStateType::FULLSCREEN:
      callbacks = &fullscreen_callbacks_;
      break;
  }
  while (!callbacks->empty()) {
    std::move(callbacks->front()).Run();
    callbacks->pop();
  }

  if (prev_state != window_state_) {
    for (const auto& monitor : monitors_) {
      monitor->OnWindowStateChanged(window_state_);
    }
  }
}

CameraAppWindowStateController::WindowStateType
CameraAppWindowStateController::GetCurrentWindowState() {
  if (widget_->IsMinimized()) {
    return WindowStateType::MINIMIZED;
  } else if (widget_->IsMaximized()) {
    return WindowStateType::MAXIMIZED;
  } else if (widget_->IsFullscreen()) {
    return WindowStateType::FULLSCREEN;
  } else {
    return WindowStateType::REGULAR;
  }
}

}  // namespace chromeos
