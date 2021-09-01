// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_policy_handler.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_installer.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/policy_util.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

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

CellularPolicyHandler::InstallPolicyESimRequest::InstallPolicyESimRequest(
    const std::string& smdp_address,
    const base::DictionaryValue& onc_config)
    : smdp_address(smdp_address), onc_config(onc_config.CreateDeepCopy()) {}

CellularPolicyHandler::InstallPolicyESimRequest::~InstallPolicyESimRequest() =
    default;

CellularPolicyHandler::CellularPolicyHandler()
    : retry_backoff_(&kRetryBackoffPolicy) {}

CellularPolicyHandler::~CellularPolicyHandler() = default;

void CellularPolicyHandler::Init(
    CellularESimInstaller* cellular_esim_installer,
    NetworkProfileHandler* network_profile_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  cellular_esim_installer_ = cellular_esim_installer;
  network_profile_handler_ = network_profile_handler;
  managed_network_configuration_handler_ =
      managed_network_configuration_handler;
}

void CellularPolicyHandler::InstallESim(
    const std::string& smdp_address,
    const base::DictionaryValue& onc_config) {
  remaining_install_requests_.push_back(
      std::make_unique<InstallPolicyESimRequest>(smdp_address, onc_config));
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
    PopRequestAndProcessNext();
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

  return remaining_install_requests_.front()->smdp_address;
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
    NET_LOG(EVENT)
        << "Successfully installed policy eSim profile on service path: "
        << *service_path << ", iccid: " << profile_properties->iccid().value();
    UpdateShillConfiguration(profile_properties->iccid().value(),
                             *service_path);
    return;
  }

  if (retry_backoff_.failure_count() >= kInstallRetryLimit) {
    NET_LOG(ERROR) << "Install policy eSim profile with SMDP: "
                   << GetCurrentSmdpAddress() << " failed three times.";
    PopRequestAndProcessNext();
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

void CellularPolicyHandler::PopRequestAndProcessNext() {
  // Pop out the completed smdp and process next request.
  remaining_install_requests_.pop_front();
  is_installing_ = false;
  ProcessRequests();
}

void CellularPolicyHandler::UpdateShillConfiguration(
    const std::string& iccid,
    const std::string& service_path) {
  DCHECK(is_installing_);

  managed_network_configuration_handler_->RemoveConfiguration(
      service_path,
      base::BindOnce(&CellularPolicyHandler::OnRemoveConfigurationSuccess,
                     weak_ptr_factory_.GetWeakPtr(), service_path, iccid),
      base::BindOnce(&CellularPolicyHandler::OnRemoveConfigurationFailure,
                     weak_ptr_factory_.GetWeakPtr(), service_path));
}

void CellularPolicyHandler::OnRemoveConfigurationSuccess(
    const std::string& service_path,
    const std::string& iccid) {
  NET_LOG(EVENT)
      << "Successfully removed cellular network configuration with path: "
      << service_path;

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(
          /*userhash=*/std::string());
  // The profile is not expected to be null, since policy applicator is not
  // started by managed network configuration handler until after a profile
  // is available.
  DCHECK(profile);

  const std::string* guid =
      remaining_install_requests_.front()->onc_config->FindStringKey(
          ::onc::network_config::kGUID);
  DCHECK(guid);

  // Insert the new ICCID into the ONC configuration
  remaining_install_requests_.front()->onc_config->SetString(
      shill::kIccidProperty, iccid);
  base::Value new_shill_properties = policy_util::CreateShillConfiguration(
      *profile, *guid, /*global_policy=*/nullptr,
      remaining_install_requests_.front()->onc_config.get(),
      /*user_settings=*/nullptr);

  managed_network_configuration_handler_->ConfigurePolicyNetwork(
      new_shill_properties,
      base::BindOnce(&CellularPolicyHandler::PopRequestAndProcessNext,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularPolicyHandler::OnRemoveConfigurationFailure(
    const std::string& service_path,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << "Failed to remove cellular network configuration with path "
                 << service_path << ". Error:" << error_name << ".";
  PopRequestAndProcessNext();
}

}  // namespace chromeos