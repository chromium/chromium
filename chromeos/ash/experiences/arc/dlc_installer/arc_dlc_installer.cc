// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_hardware_checker.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_notification_manager_factory.h"

namespace arc {

namespace {

constexpr const char kArcvmDlcId[] = "android-vm-dlc";
constexpr const char kArcvmBindMountDlcPath[] =
    "arcvm_2dbind_2dmount_2ddlc_2dpath";

}  // namespace

ArcDlcInstaller::ArcDlcInstaller(
    std::unique_ptr<ArcDlcNotificationManagerFactory>
        notification_manager_factory,
    std::unique_ptr<ArcDlcInstallHardwareChecker> hardware_checker,
    ash::CrosSettings* cros_settings)
    : notification_manager_factory_(std::move(notification_manager_factory)),
      hardware_checker_(std::move(hardware_checker)),
      cros_settings_(std::move(cros_settings)) {}

ArcDlcInstaller::~ArcDlcInstaller() = default;

void ArcDlcInstaller::PrepareArc(base::OnceCallback<void(bool)> callback) {
  if (!IsDlcRequired()) {
    return;
  }

  hardware_checker_->IsCompatible(
      base::BindOnce(&ArcDlcInstaller::OnHardwareCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDlcInstaller::OnHardwareCheckComplete(
    base::OnceCallback<void(bool)> callback,
    bool is_compatible) {
  if (!is_compatible) {
    VLOG(1) << "Device is not compatible for ARC.";
    std::move(callback).Run(false);
    return;
  }

  auto installation_triggered = std::make_unique<bool>(false);
  auto* installation_triggered_ptr = installation_triggered.get();
  dlcservice::InstallRequest install_request;
  install_request.set_id(kArcvmDlcId);

  VLOG(1) << "Device is compatible for ARC. Installing ARCVM DLC.";
  base::TimeTicks start_installation_time = base::TimeTicks::Now();
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&ArcDlcInstaller::OnDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     start_installation_time,
                     std::move(installation_triggered)),
      base::BindRepeating(&ArcDlcInstaller::OnDlcProgress,
                          weak_ptr_factory_.GetWeakPtr(),
                          installation_triggered_ptr));
}

bool ArcDlcInstaller::IsDlcRequired() {
  if (!IsArcVmDlcEnabled()) {
    return false;
  }

  if (!ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    VLOG(1) << "Device is not managed and cannot install arcvm images.";
    return false;
  }

  bool device_flex_arc_preload_enabled_allowed = false;
  if (!cros_settings_->GetBoolean(ash::kDeviceFlexArcPreloadEnabled,
                                  &device_flex_arc_preload_enabled_allowed)) {
    VLOG(1) << "Failed to get DeviceFlexArcPreloadEnabled policy; defaulting "
               "to disabled.";
    return false;
  }

  if (!device_flex_arc_preload_enabled_allowed) {
    VLOG(1) << "Reven device cannot install arcvm images because the "
               "DeviceFlexArcPreloadEnabled policy prevents it.";
    return false;
  }

  return true;
}

void ArcDlcInstaller::MaybeShowDlcInstallNotification(NotificationType type) {
  if (!arc_dlc_install_notification_manager_) {
    VLOG(1) << "Notification manager not initialized. Queueing notification.";
    dlc_install_pending_notifications_.push_back(type);
    return;
  }
  arc_dlc_install_notification_manager_->Show(type);
}

void ArcDlcInstaller::OnPrimaryUserSessionStarted(const AccountId& account_id) {
  arc_dlc_install_notification_manager_ =
      notification_manager_factory_->CreateNotificationManager(account_id);

  for (const auto& notification : dlc_install_pending_notifications_) {
    arc_dlc_install_notification_manager_->Show(notification);
  }
  dlc_install_pending_notifications_.clear();
}

void ArcDlcInstaller::OnDlcProgress(bool* installation_triggered_ptr,
                                    double progress) {
  CHECK(installation_triggered_ptr);
  if (*installation_triggered_ptr) {
    return;
  }

  MaybeShowDlcInstallNotification(NotificationType::kArcVmPreloadStarted);
  *installation_triggered_ptr = true;
}

void ArcDlcInstaller::OnDlcInstalled(
    base::OnceCallback<void(bool)> callback,
    base::TimeTicks start_installation_time,
    std::unique_ptr<bool> installation_triggered,
    const ash::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error != dlcservice::kErrorNone) {
    VLOG(1) << "Failed to install ARCVM DLC: " << install_result.error;
    MaybeShowDlcInstallNotification(NotificationType::kArcVmPreloadFailed);
    base::UmaHistogramBoolean("Arc.DlcInstaller.Install", false);
    std::move(callback).Run(false);
    return;
  }

  base::TimeDelta install_duration =
      base::TimeTicks::Now() - start_installation_time;
  base::UmaHistogramLongTimes("Arc.DlcInstaller.InstallTime", install_duration);
  base::UmaHistogramBoolean("Arc.DlcInstaller.Install", true);
  // If installation_triggered is false, the DLC image was already
  // installed, preventing a duplicate "installation complete" notification. If
  // installation_triggered is true, it means the DLC installation did not
  // complete previously; the user should receive the notification if
  // installation succeeds.
  CHECK(installation_triggered);
  if (*installation_triggered) {
    MaybeShowDlcInstallNotification(NotificationType::kArcVmPreloadSucceeded);
  }

  OnPrepareArcDlc(std::move(callback), true);
}

const std::vector<NotificationType>&
ArcDlcInstaller::GetDlcInstallPendingNotificationsForTesting() const {
  return dlc_install_pending_notifications_;
}

void ArcDlcInstaller::OnPrepareArcDlc(base::OnceCallback<void(bool)> callback,
                                      bool result) {
  if (!result) {
    LOG(ERROR) << "ARC DLC preparation failed.";
    std::move(callback).Run(false);
    return;
  }

  std::deque<JobDesc> jobs = {
      JobDesc{kArcvmBindMountDlcPath, UpstartOperation::JOB_STOP_AND_START, {}},
  };

  ConfigureUpstartJobs(
      std::move(jobs),
      base::BindOnce(std::move(callback)));  // Forward the callback
}

}  // namespace arc
