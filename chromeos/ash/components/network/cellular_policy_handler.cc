// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_policy_handler.h"

#include "base/containers/contains.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

constexpr int kInstallRetryLimit = 3;
constexpr base::TimeDelta kInstallRetryDelay = base::Days(1);

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
    policy_util::SmdxActivationCode activation_code,
    const base::Value::Dict& onc_config)
    : activation_code(std::move(activation_code)),
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
  network_state_handler_observer_.Observe(network_state_handler_.get());
}

void CellularPolicyHandler::InstallESim(const std::string& smdp_address,
                                        const base::Value::Dict& onc_config) {
  DCHECK(!ash::features::IsSmdsSupportEnabled());
  PushRequestAndProcess(std::make_unique<InstallPolicyESimRequest>(
      policy_util::SmdxActivationCode(
          policy_util::SmdxActivationCode::Type::SMDP, smdp_address),
      onc_config));
}

void CellularPolicyHandler::InstallESim(const base::Value::Dict& onc_config) {
  DCHECK(ash::features::IsSmdsSupportEnabled());

  absl::optional<policy_util::SmdxActivationCode> activation_code =
      policy_util::GetSmdxActivationCodeFromONC(onc_config);

  if (!activation_code.has_value()) {
    return;
  }

  NET_LOG(EVENT) << "Queueing a policy eSIM profile installation request with "
                 << "activation code found in the provided ONC configuration: "
                 << activation_code->ToString();

  PushRequestAndProcess(std::make_unique<InstallPolicyESimRequest>(
      std::move(activation_code.value()), onc_config));
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
  NET_LOG(EVENT) << "Installing policy eSIM profile: "
                 << GetCurrentActivationCode().ToString();
  AttemptInstallESim();
}

void CellularPolicyHandler::ScheduleRetry(
    std::unique_ptr<InstallPolicyESimRequest> request,
    InstallRetryReason reason) {
  if (reason != InstallRetryReason::kInternalError &&
      request->retry_backoff.failure_count() >= kInstallRetryLimit) {
    NET_LOG(ERROR) << "Failed to install policy eSIM profile: "
                   << request->activation_code.ToErrorString();
    ProcessRequests();
    return;
  }

  request->retry_backoff.InformOfRequest(/*succeeded=*/false);

  // Force a delay of |kInstallRetryDelay| when we fail for any reason other
  // than an internal failure, e.g. failure to inhibit, to reduce frequent
  // retries due to errors that are unlikely to be resolved quickly, e.g. an
  // invalid activation code.
  if (reason == InstallRetryReason::kOther) {
    request->retry_backoff.SetCustomReleaseTime(base::TimeTicks::Now() +
                                                kInstallRetryDelay);
  }

  const base::TimeDelta retry_delay =
      request->retry_backoff.GetTimeUntilRelease();

  NET_LOG(ERROR) << "Failed to install policy eSIM profile. Retrying in "
                 << retry_delay << ": "
                 << request->activation_code.ToErrorString();

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
        cellular_utils::GetCellularProfile(network_profile_handler_);
    DCHECK(profile);

    managed_network_configuration_handler_->OnCellularPoliciesApplied(*profile);
  }
}

void CellularPolicyHandler::AttemptInstallESim() {
  DCHECK(is_installing_);

  const DeviceState* cellular_device =
      network_state_handler_->GetDeviceStateByType(
          NetworkTypePattern::Cellular());
  if (!cellular_device) {
    // Cellular device may not be ready. Wait for DeviceListChanged notification
    // before continuing with installation.
    NET_LOG(EVENT)
        << "Waiting for the cellular device to become available to install "
        << "policy eSIM profile: " << GetCurrentActivationCode().ToString();
    wait_timer_.Start(FROM_HERE, kCellularDeviceWaitTime,
                      base::BindOnce(&CellularPolicyHandler::OnWaitTimeout,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  absl::optional<dbus::ObjectPath> euicc_path =
      cellular_utils::GetCurrentEuiccPath();
  if (!euicc_path) {
    // Hermes may not be ready and available EUICC list is empty. Wait for
    // AvailableEuiccListChanged notification to continue with installation.
    NET_LOG(EVENT) << "Waiting for EUICC to be found to install policy eSIM "
                   << "profile: " << GetCurrentActivationCode().ToString();
    wait_timer_.Start(FROM_HERE, kEuiccWaitTime,
                      base::BindOnce(&CellularPolicyHandler::OnWaitTimeout,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (need_refresh_profile_list_) {
    // Profile list for current EUICC may not have been refreshed, so explicitly
    // refresh profile list before processing installation requests.
    cellular_esim_profile_handler_->RefreshProfileListAndRestoreSlot(
        *euicc_path,
        base::BindOnce(&CellularPolicyHandler::OnRefreshProfileList,
                       weak_ptr_factory_.GetWeakPtr(), *euicc_path));
    return;
  }

  PerformInstallESim(*euicc_path);
}

void CellularPolicyHandler::PerformInstallESim(
    const dbus::ObjectPath& euicc_path) {
  base::Value::Dict new_shill_properties = GetNewShillProperties();
  absl::optional<dbus::ObjectPath> profile_path =
      FindExistingMatchingESimProfile();
  if (profile_path) {
    NET_LOG(EVENT) << "Found an existing installed profile that matches the "
                   << "policy eSIM installation request. Configuring a Shill "
                   << "service for the profile: "
                   << GetCurrentActivationCode().ToString();
    cellular_esim_installer_->ConfigureESimService(
        std::move(new_shill_properties), euicc_path, *profile_path,
        base::BindOnce(&CellularPolicyHandler::OnConfigureESimService,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!HasNonCellularInternetConnectivity()) {
    NET_LOG(ERROR)
        << "Failed to install the policy eSIM profile due to missing a "
        << "non-cellular internet connection: "
        << GetCurrentActivationCode().ToErrorString();
    auto current_request = std::move(remaining_install_requests_.front());
    PopRequest();
    ScheduleRetry(std::move(current_request),
                  InstallRetryReason::kMissingNonCellularConnectivity);
    ProcessRequests();
    return;
  }

  NET_LOG(EVENT) << "Installing policy eSIM profile: "
                 << GetCurrentActivationCode().ToString();

  // TODO(b/278135304): Implement ash::features::IsSmdsSupportEnabled().
  if (!ash::features::IsSmdsSupportEnabled()) {
    // Remote provisioning of eSIM profiles via SM-DP+ activation code in policy
    // does not require confirmation code.
    cellular_esim_installer_->InstallProfileFromActivationCode(
        GetCurrentActivationCode().value(), /*confirmation_code=*/std::string(),
        euicc_path, std::move(new_shill_properties),
        base::BindOnce(
            &CellularPolicyHandler::OnESimProfileInstallAttemptComplete,
            weak_ptr_factory_.GetWeakPtr()),
        remaining_install_requests_.front()->retry_backoff.failure_count() ==
            0);
  }
}

void CellularPolicyHandler::OnRefreshProfileList(
    const dbus::ObjectPath& euicc_path,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Failed to refresh the profile list due to an inhibit "
                   << "error, path: " << euicc_path.value();
    PerformInstallESim(euicc_path);
    return;
  }

  need_refresh_profile_list_ = false;
  // Reset the inhibit_lock so that the device will be uninhibited
  // automatically.
  inhibit_lock.reset();
  PerformInstallESim(euicc_path);
}

void CellularPolicyHandler::OnConfigureESimService(
    absl::optional<dbus::ObjectPath> service_path) {
  DCHECK(is_installing_);

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  if (!service_path) {
    ScheduleRetry(std::move(current_request), InstallRetryReason::kOther);
    ProcessRequests();
    return;
  }

  NET_LOG(EVENT) << "Successfully configured a Shill service for the existing "
                 << "profile: " << current_request->activation_code.ToString();

  current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
  const std::string* iccid =
      policy_util::GetIccidFromONC(current_request->onc_config);
  // TODO(b/278135304): Implement ash::features::IsSmdsSupportEnabled().
  if (!ash::features::IsSmdsSupportEnabled()) {
    managed_cellular_pref_handler_->AddIccidSmdpPair(
        *iccid, current_request->activation_code.value());
  }
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
    if (!base::Contains(kHermesUserErrorCodes, hermes_status)) {
      NET_LOG(ERROR) << "Failed to install the policy eSIM profile due to a "
                     << "non-user error: " << hermes_status << ". Scheduling "
                     << "another attempt: "
                     << current_request->activation_code.ToErrorString();
      ScheduleRetry(std::move(current_request),
                    base::Contains(kHermesInternalErrorCodes, hermes_status)
                        ? InstallRetryReason::kInternalError
                        : InstallRetryReason::kOther);
    } else {
      NET_LOG(ERROR)
          << "Failed to install the policy eSIM profile due to a user error: "
          << hermes_status << ". Will not schedule another attempt: "
          << current_request->activation_code.ToErrorString();
    }
    ProcessRequests();
    return;
  }

  NET_LOG(EVENT) << "Successfully installed policy eSIM profile: "
                 << current_request->activation_code.ToString();

  current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(*profile_path);
  // TODO(b/278135304): Implement ash::features::IsSmdsSupportEnabled().
  if (!ash::features::IsSmdsSupportEnabled()) {
    managed_cellular_pref_handler_->AddIccidSmdpPair(
        profile_properties->iccid().value(),
        current_request->activation_code.value(),
        /*sync_stub_networks=*/false);
  }

  managed_network_configuration_handler_->NotifyPolicyAppliedToNetwork(
      *service_path);

  ProcessRequests();
}

void CellularPolicyHandler::OnWaitTimeout() {
  NET_LOG(ERROR) << "Timed out when waiting for the EUICC or profile list.";

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  ScheduleRetry(std::move(current_request), InstallRetryReason::kInternalError);
  ProcessRequests();
}

base::Value::Dict CellularPolicyHandler::GetNewShillProperties() {
  const NetworkProfile* profile =
      cellular_utils::GetCellularProfile(network_profile_handler_);
  DCHECK(profile);

  const std::string* guid =
      remaining_install_requests_.front()->onc_config.FindString(
          ::onc::network_config::kGUID);
  DCHECK(guid);

  return policy_util::CreateShillConfiguration(
      *profile, *guid, /*global_policy=*/nullptr,
      &(remaining_install_requests_.front()->onc_config),
      /*user_settings=*/nullptr);
}

const policy_util::SmdxActivationCode&
CellularPolicyHandler::GetCurrentActivationCode() const {
  DCHECK(is_installing_);
  return remaining_install_requests_.front()->activation_code;
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

bool CellularPolicyHandler::HasNonCellularInternetConnectivity() {
  const NetworkState* default_network =
      network_state_handler_->DefaultNetwork();
  return default_network && default_network->type() != shill::kTypeCellular &&
         default_network->IsOnline();
}

}  // namespace ash
