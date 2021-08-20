// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_policy_handler.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_installer.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/network_event_log.h"

namespace chromeos {

namespace {

// Delay for first install ESim retry attempt. Delay doubles for every
// subsequent attempt.
constexpr base::TimeDelta kInstallRetryDelay = base::TimeDelta::FromMinutes(10);
const int kInstallRetryLimit = 3;

}  // namespace

CellularPolicyHandler::CellularPolicyHandler() = default;

CellularPolicyHandler::~CellularPolicyHandler() = default;

void CellularPolicyHandler::Init(
    CellularESimInstaller* cellular_esim_installer) {
  cellular_esim_installer_ = cellular_esim_installer;
}

void CellularPolicyHandler::InstallESim(const std::string& smdp_address) {
  remaining_install_requests_.push_back(smdp_address);
  ProcessRequests();
}

void CellularPolicyHandler::ProcessRequests() {
  if (remaining_install_requests_.empty())
    return;

  // Another install request is already underway; wait until it has completed
  // before starting a new request.
  if (is_installing_)
    return;

  install_attempts_so_far_ = 0;
  is_installing_ = true;
  NET_LOG(EVENT) << "Starting installing policy eSim profile with SMDP: "
                 << GetCurrentSMDPAddress();
  AttemptInstallESim();
}

void CellularPolicyHandler::AttemptInstallESim() {
  DCHECK(is_installing_);

  absl::optional<dbus::ObjectPath> euicc_path = GetCurrentEuiccPath();
  if (euicc_path == absl::nullopt) {
    NET_LOG(ERROR) << "Error installing policy eSim profile for SMDP: "
                   << GetCurrentSMDPAddress() << ": euicc is not found.";
    CompleteCurrentRequest(absl::nullopt);
    return;
  }

  NET_LOG(EVENT) << "Attempt installing policy eSim profile with SMDP: "
                 << GetCurrentSMDPAddress()
                 << " on euicc path: " << euicc_path.value().value();
  cellular_esim_installer_->InstallProfileFromActivationCode(
      GetCurrentSMDPAddress(), std::string(), euicc_path.value(),
      base::BindOnce(&CellularPolicyHandler::OnESimProfileInstalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

const std::string& CellularPolicyHandler::GetCurrentSMDPAddress() const {
  DCHECK(is_installing_);

  return remaining_install_requests_.front();
}

void CellularPolicyHandler::OnESimProfileInstalled(
    HermesResponseStatus hermes_status,
    absl::optional<dbus::ObjectPath> profile_path) {
  DCHECK(is_installing_);

  if (hermes_status == HermesResponseStatus::kSuccess) {
    DCHECK(profile_path != absl::nullopt);
    DCHECK(profile_path.value().IsValid());
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path.value());
    DCHECK(profile_properties);
    CompleteCurrentRequest(profile_properties->iccid().value());
    return;
  }

  if (install_attempts_so_far_ >= kInstallRetryLimit) {
    NET_LOG(ERROR) << "Install policy eSim profile with SMDP: "
                   << GetCurrentSMDPAddress() << " failed three times.";
    CompleteCurrentRequest(absl::nullopt);
    return;
  }
  base::TimeDelta retry_delay =
      kInstallRetryDelay * (1 << install_attempts_so_far_);
  NET_LOG(EVENT) << "Install policy eSim profile failed. Retrying in "
                 << retry_delay;
  install_attempts_so_far_++;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CellularPolicyHandler::AttemptInstallESim,
                     weak_ptr_factory_.GetWeakPtr()),
      retry_delay);
}

void CellularPolicyHandler::CompleteCurrentRequest(
    absl::optional<const std::string> iccid) {
  DCHECK(is_installing_);

  // TODO(crbug.com/1231305)
  // If succeed: delete current shill property entry and create new shill
  // entry with policy configuration and new ICCID.

  // Pop out the completed smdp and process next request.
  remaining_install_requests_.pop_front();
  is_installing_ = false;
  ProcessRequests();
}

}  // namespace chromeos