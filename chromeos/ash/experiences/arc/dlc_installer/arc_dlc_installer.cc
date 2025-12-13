// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/experiences/arc/arc_platform_support.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"

namespace arc {

namespace {

constexpr const char kArcvmDlcId[] = "android-vm-dlc";
constexpr const char kArcvmBindMountDlcPath[] =
    "arcvm_2dbind_2dmount_2ddlc_2dpath";

void OnGetDlcState(base::OnceCallback<void(ArcDlcInstaller::DlcState)> callback,
                   std::string_view err,
                   const dlcservice::DlcState& dlc_state) {
  if (err != dlcservice::kErrorNone) {
    LOG(ERROR) << "GetDlcState failed for " << kArcvmDlcId << ": " << err;
    std::move(callback).Run(ArcDlcInstaller::DlcState::kError);
    return;
  }

  if (dlc_state.state() == dlcservice::DlcState::INSTALLED) {
    std::move(callback).Run(ArcDlcInstaller::DlcState::kInstalled);
  } else {
    std::move(callback).Run(ArcDlcInstaller::DlcState::kNotInstalled);
  }
}

void OnDlcServiceAvailableForStateCheck(
    base::OnceCallback<void(ArcDlcInstaller::DlcState)> callback,
    bool available) {
  if (!available) {
    LOG(ERROR) << "dlcservice not available.";
    std::move(callback).Run(ArcDlcInstaller::DlcState::kError);
    return;
  }
  ash::DlcserviceClient::Get()->GetDlcState(
      kArcvmDlcId, base::BindOnce(&OnGetDlcState, std::move(callback)));
}

}  // namespace

ArcDlcInstaller::ArcDlcInstaller() = default;

ArcDlcInstaller::~ArcDlcInstaller() = default;

void ArcDlcInstaller::PrepareArc(base::OnceCallback<void(bool)> callback) {
  if (!arc::ArcPlatformSupport::Get()->IsDlcEnabled()) {
    VLOG(1) << "The device is not managed or the preload policy is not "
               "enabled, so the ARCVM image from DLC is not required.";
    std::move(callback).Run(false);
    return;
  }

  if (!IsArcVmDlcHardwareRequirementSatisfied()) {
    VLOG(1) << "The device does not meet the minimum hardware requirements to "
               "install the ARCVM image from a DLC.";
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << "Device is allowed to install ARCVM image from DLC. Checking DLC"
             "service.";
  prepare_arc_callback_ = std::move(callback);
  ash::DlcserviceClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &ArcDlcInstaller::OnDlcServiceAvailable, weak_ptr_factory_.GetWeakPtr()));
}

void ArcDlcInstaller::CheckInstallationState(
    base::OnceCallback<void(DlcState)> callback) {
  if (!arc::ArcPlatformSupport::Get()->IsDlcEnabled()) {
    std::move(callback).Run(DlcState::kNotRequired);
    return;
  }

  ash::DlcserviceClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&OnDlcServiceAvailableForStateCheck, std::move(callback)));
}

void ArcDlcInstaller::OnDlcServiceAvailable(bool service_available) {
  DCHECK(prepare_arc_callback_);

  if (!service_available) {
    LOG(ERROR) << "DLC service is not available, cannot install ARCVM DLC.";
    arc_dlc_install_notification_manager::Show(
        arc_dlc_install_notification_manager::NotificationType::
            kArcVmPreloadFailed);
    base::UmaHistogramBoolean("Arc.DlcInstaller.Install", false);
    std::move(prepare_arc_callback_).Run(false);
    return;
  }

  auto installation_triggered = std::make_unique<bool>(false);
  auto* installation_triggered_ptr = installation_triggered.get();
  dlcservice::InstallRequest install_request;
  install_request.set_id(kArcvmDlcId);

  VLOG(1) << "Device service is available. Installing ARCVM DLC.";
  base::TimeTicks start_installation_time = base::TimeTicks::Now();
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&ArcDlcInstaller::OnDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(prepare_arc_callback_), start_installation_time,
                     std::move(installation_triggered)),
      base::BindRepeating(&ArcDlcInstaller::OnDlcProgress,
                          weak_ptr_factory_.GetWeakPtr(),
                          installation_triggered_ptr));
}

void ArcDlcInstaller::OnDlcProgress(bool* installation_triggered_ptr,
                                    double progress) {
  CHECK(installation_triggered_ptr);
  if (*installation_triggered_ptr) {
    return;
  }

  arc_dlc_install_notification_manager::Show(
      arc_dlc_install_notification_manager::NotificationType::
          kArcVmPreloadStarted);
  *installation_triggered_ptr = true;
}

void ArcDlcInstaller::OnDlcInstalled(
    base::OnceCallback<void(bool)> callback,
    base::TimeTicks start_installation_time,
    std::unique_ptr<bool> installation_triggered,
    const ash::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error != dlcservice::kErrorNone) {
    VLOG(1) << "Failed to install ARCVM DLC: " << install_result.error;
    arc_dlc_install_notification_manager::Show(
        arc_dlc_install_notification_manager::NotificationType::
            kArcVmPreloadFailed);
    base::UmaHistogramBoolean("Arc.DlcInstaller.Install", false);
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << "ARCVM DLC installed successfully.";
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
    arc_dlc_install_notification_manager::Show(
        arc_dlc_install_notification_manager::NotificationType::
            kArcVmPreloadSucceeded);
  }

  OnPrepareArcDlc(std::move(callback), true);
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
