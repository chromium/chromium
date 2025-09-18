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
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"

namespace ash {
class CrosSettings;
}

namespace base {
class TimeTicks;
}

namespace arc {

// The ArcDlcInstaller class manages the installation process for ARC DLC
// (Android Runtime for Chrome). It manages notifications related to the
// installation, and facilitates enabling ARC on devices.
class ArcDlcInstaller {
 public:
  explicit ArcDlcInstaller(ash::CrosSettings* cros_settings);

  ArcDlcInstaller(const ArcDlcInstaller&) = delete;
  ArcDlcInstaller& operator=(const ArcDlcInstaller&) = delete;

  ~ArcDlcInstaller();

  // Checks if ARC should be enabled on a device. If the device needs the
  // DLC.
  void PrepareArc(base::OnceCallback<void(bool)> callback);

  // Determines if the DLC installation is necessary based on
  // board, management, and feature flag conditions.
  bool IsDlcRequired();

 private:
  // Callback invoked after ARC DLC preparation is complete.
  // This function is triggered when the DLC required for
  // ARC has been prepared successfully.
  // If the preparation succeeds, this function proceeds to
  // configure the necessary Upstart jobs that bind mount the DLC image and set
  // up the ARC environment.
  void OnPrepareArcDlc(base::OnceCallback<void(bool)> callback, bool result);

  // Called when the availability of the DLC service is determined. If
  // available, this function initiates the installation of the ARCVM DLC.
  void OnDlcServiceAvailable(bool service_available);

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

  raw_ptr<ash::CrosSettings> cros_settings_;
  base::OnceCallback<void(bool)> prepare_arc_callback_;
  base::WeakPtrFactory<ArcDlcInstaller> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALLER_H_
