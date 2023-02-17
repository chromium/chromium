// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_policy_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/value_iterators.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const int kInstallRetryLimit = 10;

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,               // Number of initial errors to ignore.
    5 * 60 * 1000,   // Initial delay of 5 minutes in ms.
    2.0,             // Factor by which the waiting time will be multiplied.
    0,               // Fuzzing percentage.
    60 * 60 * 1000,  // Maximum delay of 1 hour in ms.
    -1,              // Never discard the entry.
    true,            // Use initial delay.
};

// Timeout waiting for EUICC to become available in Hermes.
constexpr base::TimeDelta kEuiccWaitTime = base::Minutes(3);
// Timeout waiting for cellular device to become available.
constexpr base::TimeDelta kCellularDeviceWaitTime = base::Seconds(30);

}  // namespace

CellularPolicyHandler::InstallPolicyESimRequest::InstallPolicyESimRequest(
    const std::string& smdp_address,
    const base::Value::Dict& onc_config)
    : smdp_address(smdp_address),
      onc_config(onc_config.Clone()),
      retry_backoff(&kRetryBackoffPolicy) {}

CellularPolicyHandler::InstallPolicyESimRequest::~InstallPolicyESimRequest() =
    default;

CellularPolicyHandler::CellularPolicyHandler() = default;

CellularPolicyHandler::~CellularPolicyHandler() {
  OnShuttingDown();
}

void CellularPolicyHandler::Init(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    CellularESimInstaller* cellular_esim_installer,
    NetworkProfileHandler* network_profile_handler,
    NetworkStateHandler* network_state_handler,
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  cellular_esim_installer_ = cellular_esim_installer;
  network_profile_handler_ = network_profile_handler;
  network_state_handler_ = network_state_handler;
  managed_cellular_pref_handler_ = managed_cellular_pref_handler;
  managed_network_configuration_handler_ =
      managed_network_configuration_handler;

  hermes_observation_.Observe(HermesManagerClient::Get());
  cellular_esim_profile_handler_observation_.Observe(
      cellular_esim_profile_handler);
  network_state_handler_observer_.Observe(network_state_handler_);
}

void CellularPolicyHandler::InstallESim(const std::string& smdp_address,
                                        const base::Value::Dict& onc_config) {
  PushRequestAndProcess(
      std::make_unique<InstallPolicyESimRequest>(smdp_address, onc_config));
}

void CellularPolicyHandler::OnAvailableEuiccListChanged() {
  ResumeInstallIfNeeded();
}

void CellularPolicyHandler::OnESimProfileListUpdated() {
  ResumeInstallIfNeeded();
}

void CellularPolicyHandler::DeviceListChanged() {
  ResumeInstallIfNeeded();
}

void CellularPolicyHandler::OnShuttingDown() {
  if (!network_state_handler_) {
    return;
  }
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
}

void CellularPolicyHandler::ResumeInstallIfNeeded() {
  if (!is_installing_ || !wait_timer_.IsRunning()) {
    return;
  }
  wait_timer_.Stop();
  AttemptInstallESim();
}

void CellularPolicyHandler::ProcessRequests() {
  if (remaining_install_requests_.empty()) {
    need_refresh_profile_list_ = true;
    return;
  }

  // Another install request is already underway; wait until it has completed
  // before starting a new request.
  if (is_installing_)
    return;

  is_installing_ = true;
  NET_LOG(EVENT)
      << "Starting installing policy eSIM profile with SMDP address: "
      << GetCurrentSmdpAddress();
  AttemptInstallESim();
}

void CellularPolicyHandler::AttemptInstallESim() {
  DCHECK(is_installing_);

  const DeviceState* cellular_device =
      network_state_handler_->GetDeviceStateByType(
          NetworkTypePattern::Cellular());
  if (!cellular_device) {
    // Cellular device may not be ready. Wait for DeviceListChanged notification
    // before continuing with installation.
    NET_LOG(EVENT) << "Cellular device is not available when attempting to "
                   << "install eSIM profile for SMDP address: "
                   << GetCurrentSmdpAddress()
                   << ". Waiting for device list change.";
    wait_timer_.Start(FROM_HERE, kCellularDeviceWaitTime,
                      base::BindOnce(&CellularPolicyHandler::OnWaitTimeout,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  absl::optional<dbus::ObjectPath> euicc_path = GetCurrentEuiccPath();
  if (!euicc_path) {
    // Hermes may not be ready and available euicc list is empty. Wait for
    // AvailableEuiccListChanged notification to continue with installation.
    NET_LOG(EVENT) << "No EUICC found when attempting to install eSIM profile "
                   << "for SMDP address: " << GetCurrentSmdpAddress()
                   << ". Waiting for EUICC.";
    wait_timer_.Start(FROM_HERE, kEuiccWaitTime,
                      base::BindOnce(&CellularPolicyHandler::OnWaitTimeout,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (need_refresh_profile_list_) {
    // Profile list for current euicc may not have been refreshed, so explicitly
    // refresh profile list before processing installation requests.
    cellular_esim_profile_handler_->RefreshProfileListAndRestoreSlot(
        *euicc_path,
        base::BindOnce(&CellularPolicyHandler::OnRefreshProfileList,
                       weak_ptr_factory_.GetWeakPtr(), *euicc_path));
    return;
  }

  SetupESim(*euicc_path);
}

void CellularPolicyHandler::OnRefreshProfileList(
    const dbus::ObjectPath& euicc_path,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Refresh profile list failed due to inhibit error, path: "
                   << euicc_path.value();
    SetupESim(euicc_path);
    return;
  }

  need_refresh_profile_list_ = false;
  // Reset the inhibit_lock so that the device will be uninhibited
  // automatically.
  inhibit_lock.reset();
  SetupESim(euicc_path);
}

void CellularPolicyHandler::SetupESim(const dbus::ObjectPath& euicc_path) {
  NET_LOG(EVENT) << "Attempt setup policy eSIM profile with SMDP address: "
                 << GetCurrentSmdpAddress()
                 << " on euicc path: " << euicc_path.value();
  base::Value::Dict new_shill_properties = GetNewShillProperties();
  absl::optional<dbus::ObjectPath> profile_path =
      FindExistingMatchingESimProfile();
  if (profile_path) {
    NET_LOG(EVENT)
        << "Setting up network for existing eSIM profile with properties: "
        << new_shill_properties;
    cellular_esim_installer_->ConfigureESimService(
        std::move(new_shill_properties), euicc_path, *profile_path,
        base::BindOnce(&CellularPolicyHandler::OnConfigureESimService,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!HasNonCellularInternetConnectivity()) {
    NET_LOG(ERROR) << "No non-cellular type Internet connectivity.";
    auto current_request = std::move(remaining_install_requests_.front());
    PopRequest();
    ScheduleRetry(std::move(current_request));
    ProcessRequests();
    return;
  }

  NET_LOG(EVENT) << "Install policy eSIM profile with properties: "
                 << new_shill_properties;
  // Remote provisioning of eSIM profiles via SMDP address in policy does not
  // require confirmation code.
  cellular_esim_installer_->InstallProfileFromActivationCode(
      GetCurrentSmdpAddress(), /*confirmation_code=*/std::string(), euicc_path,
      std::move(new_shill_properties),
      base::BindOnce(
          &CellularPolicyHandler::OnESimProfileInstallAttemptComplete,
          weak_ptr_factory_.GetWeakPtr()),
      remaining_install_requests_.front()->retry_backoff.failure_count() == 0);
}

base::Value::Dict CellularPolicyHandler::GetNewShillProperties() {
  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(
          /*userhash=*/std::string());
  const std::string* guid =
      remaining_install_requests_.front()->onc_config.FindString(
          ::onc::network_config::kGUID);
  DCHECK(guid);

  return policy_util::CreateShillConfiguration(
      *profile, *guid, /*global_policy=*/nullptr,
      &(remaining_install_requests_.front()->onc_config),
      /*user_settings=*/nullptr);
}

const std::string& CellularPolicyHandler::GetCurrentSmdpAddress() const {
  DCHECK(is_installing_);

  return remaining_install_requests_.front()->smdp_address;
}

void CellularPolicyHandler::OnConfigureESimService(
    absl::optional<dbus::ObjectPath> service_path) {
  DCHECK(is_installing_);

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  if (!service_path) {
    ScheduleRetry(std::move(current_request));
    ProcessRequests();
    return;
  }

  NET_LOG(EVENT) << "Successfully configured service for existing eSIM profile";
  current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
  const std::string* iccid =
      policy_util::GetIccidFromONC(current_request->onc_config);
  managed_cellular_pref_handler_->AddIccidSmdpPair(
      *iccid, current_request->smdp_address);
  ProcessRequests();
}

void CellularPolicyHandler::OnESimProfileInstallAttemptComplete(
    HermesResponseStatus hermes_status,
    absl::optional<dbus::ObjectPath> profile_path,
    absl::optional<std::string> service_path) {
  DCHECK(is_installing_);

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  if (hermes_status != HermesResponseStatus::kSuccess) {
    ScheduleRetry(std::move(current_request));
    ProcessRequests();
    return;
  }

  NET_LOG(EVENT) << "Successfully installed eSIM profile with SMDP address: "
                 << current_request->smdp_address;
  current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(*profile_path);
  managed_cellular_pref_handler_->AddIccidSmdpPair(
      profile_properties->iccid().value(), current_request->smdp_address,
      /*sync_stub_networks=*/false);

  managed_network_configuration_handler_->NotifyPolicyAppliedToNetwork(
      *service_path);

  ProcessRequests();
}

void CellularPolicyHandler::ScheduleRetry(
    std::unique_ptr<InstallPolicyESimRequest> request) {
  if (request->retry_backoff.failure_count() >= kInstallRetryLimit) {
    NET_LOG(ERROR) << "Install policy eSIM profile with SMDP address: "
                   << request->smdp_address << " failed " << kInstallRetryLimit
                   << " times.";
    ProcessRequests();
    return;
  }

  request->retry_backoff.InformOfRequest(/*succeeded=*/false);
  base::TimeDelta retry_delay = request->retry_backoff.GetTimeUntilRelease();
  NET_LOG(ERROR) << "Install policy eSIM profile failed. Retrying in "
                 << retry_delay;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CellularPolicyHandler::PushRequestAndProcess,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)),
      retry_delay);
}

void CellularPolicyHandler::PushRequestAndProcess(
    std::unique_ptr<InstallPolicyESimRequest> request) {
  remaining_install_requests_.push_back(std::move(request));
  ProcessRequests();
}

void CellularPolicyHandler::PopRequest() {
  remaining_install_requests_.pop_front();
  is_installing_ = false;
  if (remaining_install_requests_.empty()) {
    const NetworkProfile* profile =
        network_profile_handler_->GetProfileForUserhash(
            /*userhash=*/std::string());
    DCHECK(profile);

    managed_network_configuration_handler_->OnCellularPoliciesApplied(*profile);
  }
}

absl::optional<dbus::ObjectPath>
CellularPolicyHandler::FindExistingMatchingESimProfile() {
  const std::string* iccid = policy_util::GetIccidFromONC(
      remaining_install_requests_.front()->onc_config);
  if (!iccid) {
    return absl::nullopt;
  }
  for (CellularESimProfile esim_profile :
       cellular_esim_profile_handler_->GetESimProfiles()) {
    if (esim_profile.iccid() == *iccid) {
      return esim_profile.path();
    }
  }
  return absl::nullopt;
}

void CellularPolicyHandler::OnWaitTimeout() {
  NET_LOG(ERROR) << "Install request for SMDP address: "
                 << GetCurrentSmdpAddress()
                 << ". Timed out waiting for EUICC or profile list.";
  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  ScheduleRetry(std::move(current_request));
  ProcessRequests();
}

bool CellularPolicyHandler::HasNonCellularInternetConnectivity() {
  const NetworkState* default_network =
      network_state_handler_->DefaultNetwork();
  return default_network && default_network->type() != shill::kTypeCellular &&
         default_network->IsOnline();
}

}  // namespace ash
