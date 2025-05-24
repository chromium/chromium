// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALLER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

class AccountId;

namespace ash {
class CrosSettings;
}

namespace arc {

class ArcDlcInstallHardwareChecker;
class ArcDlcInstallNotificationManager;
class ArcDlcNotificationManagerFactory;

enum class NotificationType;

// The ArcDlcInstaller class manages the installation process for ARC DLC
// (Android Runtime for Chrome). It handles hardware compatibility checks,
// manages notifications related to the installation, and facilitates
// enabling ARC on devices.
class ArcDlcInstaller {
 public:
  ArcDlcInstaller(
      std::unique_ptr<ArcDlcNotificationManagerFactory>
          notification_manager_factory,
      std::unique_ptr<ArcDlcInstallHardwareChecker> hardware_checker,
      ash::CrosSettings* cros_settings);

  ArcDlcInstaller(const ArcDlcInstaller&) = delete;
  ArcDlcInstaller& operator=(const ArcDlcInstaller&) = delete;

  ~ArcDlcInstaller();

  // Checks if ARC should be enabled on a device. If the device needs the
  // DLC, it performs a hardware compatibility check.
  void PrepareArc(base::OnceCallback<void(bool)> callback);

  // ArcServiceLauncher will invoke this function when the profile is set up,
  // and it will create the notification manager and process any pending DLC
  // installation notifications that need to be shown.
  void OnPrimaryUserSessionStarted(const AccountId& account_id);

  // Determines if the DLC installation is necessary based on
  // board, management, and feature flag conditions.
  bool IsDlcRequired();

  // Returns dlc_install_pending_notifications_ for testing.
  const std::vector<NotificationType>&
  GetDlcInstallPendingNotificationsForTesting() const;

 private:
  // Callback invoked after ARC DLC preparation is complete.
  // This function is triggered when the DLC required for
  // ARC has been prepared successfully.
  // If the preparation succeeds, this function proceeds to
  // configure the necessary Upstart jobs that bind mount the DLC image and set
  // up the ARC environment.
  void OnPrepareArcDlc(base::OnceCallback<void(bool)> callback, bool result);

  // Handles the completion of the hardware compatibility check for ARC on
  // devices. If compatible, logs the compatibility, displays a preload
  // notification, and initiates the installation of the ARCVM DLC. If not
  // compatible, logs the incompatibility and invokes the callback with a
  // failure status.
  void OnHardwareCheckComplete(base::OnceCallback<void(bool)> callback,
                               bool is_compatible);

  // Displays a DLC installation notification of the specified type if the
  // Notification Manager is initialized. If not initialized, queues the
  // notification to be shown later once the manager is ready.
  void MaybeShowDlcInstallNotification(NotificationType type);

  // Handles the result of the ARCVM DLC installation. If successful, shows a
  // success notification, configures and starts necessary Upstart jobs, and
  // invokes the callback. If installation fails, logs the error, shows a
  // failure notification, and invokes the callback with a failure status.
  void OnDlcInstalled(
      base::OnceCallback<void(bool)> callback,
      base::TimeTicks start_installation_time,
      std::unique_ptr<bool> installation_triggered,
      const ash::DlcserviceClient::InstallResult& install_result);

  // The progress_callback function in DlcserviceClient's Install() is triggered
  // only while the DLC is installing. Once the DLC image is installed, this
  // callback will no longer be triggered. The service uses this mechanism to
  // determine whether the DLC image was installed.
  void OnDlcProgress(bool* installation_triggered, double progress);

  std::unique_ptr<ArcDlcNotificationManagerFactory>
      notification_manager_factory_;
  std::unique_ptr<ArcDlcInstallNotificationManager>
      arc_dlc_install_notification_manager_;
  std::unique_ptr<ArcDlcInstallHardwareChecker> hardware_checker_;
  raw_ptr<ash::CrosSettings> cros_settings_;
  std::vector<NotificationType> dlc_install_pending_notifications_;
  base::WeakPtrFactory<ArcDlcInstaller> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALLER_H_
