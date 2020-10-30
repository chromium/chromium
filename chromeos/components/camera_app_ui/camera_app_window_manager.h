// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_H_

#include "base/containers/flat_map.h"
#include "chromeos/components/camera_app_ui/camera_app_helper.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace chromeos {

// A manager to manage the camera usage ownership between multiple camera app
// windows. The windows share the same window manager if they are under same
// browser context.
class CameraAppWindowManager : public KeyedService,
                               public views::WidgetObserver {
 public:
  CameraAppWindowManager();
  CameraAppWindowManager(const CameraAppWindowManager&) = delete;
  CameraAppWindowManager& operator=(const CameraAppWindowManager&) = delete;
  ~CameraAppWindowManager() override;

  void SetCameraUsageMonitor(
      aura::Window* window,
      mojo::PendingRemote<chromeos_camera::mojom::CameraUsageOwnershipMonitor>
          usage_monitor,
      base::OnceCallback<void()> callback);

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  enum class TransferState {
    kIdle,
    kSuspending,
    kResuming,
  };
  void OnMonitorMojoConnectionError(views::Widget* widget);
  void SuspendCameraUsage();
  void OnSuspendedCameraUsage(views::Widget* prev_owner);
  void ResumeCameraUsage();
  void OnResumedCameraUsage(views::Widget* prev_owner);
  void ResumeNextOrIdle();

  // KeyedService:
  void Shutdown() override;

  base::flat_map<
      views::Widget*,
      mojo::Remote<chromeos_camera::mojom::CameraUsageOwnershipMonitor>>
      camera_usage_monitors_;

  // Whether the |owner_| is transferring the camera usage.
  TransferState transfer_state_ = TransferState::kIdle;

  // The widget which has the camera usage ownership currently.
  views::Widget* owner_ = nullptr;

  // For the pending camera usage owner, there are three possible values:
  // 1. base::nullopt: When there is no pending owner. Transfer can stop.
  // 2. nullptr:       When there should be no active window after the transfer
  //                   is stopped.
  // 3. non-null:      When there is another window which should own camera
  //                   usage.
  base::Optional<views::Widget*> pending_transfer_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_H_
