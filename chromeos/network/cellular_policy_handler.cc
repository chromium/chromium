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

const int kInstallRetryLimit = 3;

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,               // Number of initial errors to ignore.
    5 * 60 * 1000,   // Initial delay of 5 minutes in ms.
    2.0,             // Factor by which the waiting time will be multiplied.
    0,               // Fuzzing percentage.
    60 * 60 * 1000,  // Maximum delay of 1 hour in ms.
    -1,              // Never discard the entry.
    true,            // Use initial delay.
};

}  // namespace

CellularPolicyHandler::CellularPolicyHandler()
    : retry_backoff_(&kRetryBackoffPolicy) {}

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

  // Reset the state of the backoff so that the next backoff retry starts at
  // the default initial delay.
  retry_backoff_.Reset();

  is_installing_ = true;
  NET_LOG(EVENT) << "Starting installing policy eSim profile with SMDP: "
                 << GetCurrentSmdpAddress();
  AttemptInstallESim();
}

void CellularPolicyHandler::AttemptInstallESim() {
  DCHECK(is_installing_);

  absl::optional<dbus::ObjectPath> euicc_path = GetCurrentEuiccPath();
  if (!euicc_path) {
    NET_LOG(ERROR) << "Error installing policy eSim profile for SMDP: "
                   << GetCurrentSmdpAddress() << ": euicc is not found.";
    CompleteCurrentRequest(/*iccid=*/absl::nullopt);
    return;
  }

  NET_LOG(EVENT) << "Attempt installing policy eSim profile with SMDP: "
                 << GetCurrentSmdpAddress()
                 << " on euicc path: " << euicc_path->value();
  // Remote provisioning of eSIM profiles via SMDP address in policy does not
  // require confirmation code.
  cellular_esim_installer_->InstallProfileFromActivationCode(
      GetCurrentSmdpAddress(), /*confirmation_code=*/std::string(), *euicc_path,
      base::BindOnce(
          &CellularPolicyHandler::OnESimProfileInstallAttemptComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

const std::string& CellularPolicyHandler::GetCurrentSmdpAddress() const {
  DCHECK(is_installing_);

  return remaining_install_requests_.front();
}

void CellularPolicyHandler::OnESimProfileInstallAttemptComplete(
    HermesResponseStatus hermes_status,
    absl::optional<dbus::ObjectPath> profile_path,
    absl::optional<std::string> service_path) {
  DCHECK(is_installing_);

  if (hermes_status == HermesResponseStatus::kSuccess) {
    retry_backoff_.InformOfRequest(/*succeeded=*/true);
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(*profile_path);
    CompleteCurrentRequest(profile_properties->iccid().value());
    return;
  }

  if (retry_backoff_.failure_count() >= kInstallRetryLimit) {
    NET_LOG(ERROR) << "Install policy eSim profile with SMDP: "
                   << GetCurrentSmdpAddress() << " failed three times.";
    CompleteCurrentRequest(/*iccid=*/absl::nullopt);
    return;
  }

  retry_backoff_.InformOfRequest(/*succeeded=*/false);
  NET_LOG(EVENT) << "Install policy eSim profile failed. Retrying in "
                 << retry_backoff_.GetTimeUntilRelease();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CellularPolicyHandler::AttemptInstallESim,
                     weak_ptr_factory_.GetWeakPtr()),
      retry_backoff_.GetTimeUntilRelease());
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