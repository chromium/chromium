// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_policy_handler.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/value_iterators.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_esim_profile_waiter.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ash::cellular_setup::mojom::ProfileInstallMethod;

namespace ash {

namespace {

constexpr int kInstallRetryLimit = 3;
constexpr base::TimeDelta kInstallRetryDelay = base::Days(1);

// The timeout that is provided when waiting for the properties of discovered
// pending profiles to be set before choosing a profile to install.
constexpr base::TimeDelta kCellularESimProfileWaiterDelay = base::Seconds(30);

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

// Returns the activation code for the first eSIM profile whose properties are
// available and whose state is pending, if any.
std::optional<std::string> GetFirstActivationCode(
    const dbus::ObjectPath& euicc_path,
    const std::vector<dbus::ObjectPath>& profile_paths) {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  DCHECK(euicc_properties);

  for (const auto& profile_path : profile_paths) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    if (!profile_properties) {
      NET_LOG(ERROR)
          << "Failed to get profile properties for available profile";
      continue;
    }
    if (profile_properties->state().value() !=
        hermes::profile::State::kPending) {
      NET_LOG(ERROR) << "Expected available profile to have state "
                     << hermes::profile::State::kPending << ", has "
                     << profile_properties->state().value();
      continue;
    }
    const std::string& activation_code =
        profile_properties->activation_code().value();
    if (activation_code.empty()) {
      NET_LOG(ERROR) << "Expected available profile to have an activation code";
      continue;
    }
    return activation_code;
  }
  return std::nullopt;
}

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
    CellularInhibitor* cellular_inhibitor,
    NetworkProfileHandler* network_profile_handler,
    NetworkStateHandler* network_state_handler,
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  cellular_esim_installer_ = cellular_esim_installer;
  cellular_inhibitor_ = cellular_inhibitor;
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

void CellularPolicyHandler::InstallESim(const base::Value::Dict& onc_config) {
  std::optional<policy_util::SmdxActivationCode> activation_code =
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

void CellularPolicyHandler::ScheduleRetryAndProcessRequests(
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

  ProcessRequests();
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

void CellularPolicyHandler::PopAndProcessRequests() {
  PopRequest();
  ProcessRequests();
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

  std::optional<dbus::ObjectPath> euicc_path =
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

  base::Value::Dict new_shill_properties = GetNewShillProperties();
  // If iccid is found in policy onc, the installation will be skipped because
  // it indicates that the eSIM profile has already been installed before using
  // the same SM-DP+ or SM-DS.
  const std::optional<std::string> iccid = GetIccidFromPolicyONC();
  if (iccid) {
    const std::optional<dbus::ObjectPath> profile_path =
        FindExistingMatchingESimProfile(*iccid);
    if (profile_path) {
      NET_LOG(EVENT) << "Found an existing installed profile that matches the "
                     << "policy eSIM installation request. Configuring a Shill "
                     << "service for the profile: "
                     << GetCurrentActivationCode().ToString();
      cellular_esim_installer_->ConfigureESimService(
          std::move(new_shill_properties), *euicc_path, *profile_path,
          base::BindOnce(&CellularPolicyHandler::OnConfigureESimService,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    NET_LOG(EVENT) << "Skip installation because iccid is found in the policy"
                   << " ONC, this indicates that the eSIM profile has already"
                   << " been installed.";
    PopAndProcessRequests();
    return;
  }

  if (!HasNonCellularInternetConnectivity()) {
    NET_LOG(ERROR)
        << "Failed to install the policy eSIM profile due to missing a "
        << "non-cellular internet connection: "
        << GetCurrentActivationCode().ToErrorString();
    auto current_request = std::move(remaining_install_requests_.front());
    PopRequest();
    ScheduleRetryAndProcessRequests(
        std::move(current_request),
        InstallRetryReason::kMissingNonCellularConnectivity);
    return;
  }

  if (need_refresh_profile_list_) {
    // Profile list for current EUICC may not have been refreshed, so explicitly
    // refresh profile list before processing installation requests.
    cellular_esim_profile_handler_->RefreshProfileListAndRestoreSlot(
        *euicc_path,
        base::BindOnce(&CellularPolicyHandler::OnRefreshProfileList,
                       weak_ptr_factory_.GetWeakPtr(), *euicc_path,
                       std::move(new_shill_properties)));
    return;
  }

  PerformInstallESim(*euicc_path, std::move(new_shill_properties));
}

void CellularPolicyHandler::PerformInstallESim(
    const dbus::ObjectPath& euicc_path,
    base::Value::Dict new_shill_properties) {
  NET_LOG(EVENT) << "Installing policy eSIM profile ("
                 << GetCurrentActivationCode().ToString() << ") and inhibiting "
                 << "cellular device to request available profiles for SM-DX "
                    "activation code.";

  // Confirmation codes are not required when installing policy eSIM profiles.
  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRequestingAvailableProfiles,
      base::BindOnce(&CellularPolicyHandler::OnInhibitedForRefreshSmdxProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     std::move(new_shill_properties)));
}

void CellularPolicyHandler::OnRefreshProfileList(
    const dbus::ObjectPath& euicc_path,
    base::Value::Dict new_shill_properties,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Failed to refresh the profile list due to an inhibit "
                   << "error, path: " << euicc_path.value();
  } else {
    need_refresh_profile_list_ = false;
    // Reset the |inhibit_lock| so that the device will no longer be inhibited
    // with |kRefreshingProfileList| and can become inhibited with the correct
    // reason e.g., |kInstallingProfile|.
    inhibit_lock.reset();
  }

  PerformInstallESim(euicc_path, std::move(new_shill_properties));
}

void CellularPolicyHandler::OnConfigureESimService(
    std::optional<dbus::ObjectPath> service_path) {
  DCHECK(is_installing_);

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  if (!service_path) {
    ScheduleRetryAndProcessRequests(std::move(current_request),
                                    InstallRetryReason::kOther);
    return;
  }

  NET_LOG(EVENT) << "Successfully configured a Shill service for the existing "
                 << "profile: " << current_request->activation_code.ToString();

  current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
  const std::string* iccid =
      policy_util::GetIccidFromONC(current_request->onc_config);
  DCHECK(iccid);

  const std::string* name =
      current_request->onc_config.FindString(::onc::network_config::kName);
  DCHECK(name);
  managed_cellular_pref_handler_->AddESimMetadata(
      *iccid, *name, current_request->activation_code);
  ProcessRequests();
}

void CellularPolicyHandler::OnInhibitedForRefreshSmdxProfiles(
    const dbus::ObjectPath& euicc_path,
    base::Value::Dict new_shill_properties,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  // The cellular device must be inhibited before refreshing SM-DX profiles. If
  // we fail to receive a lock consider this installation attempt a failure.
  if (!inhibit_lock) {
    NET_LOG(ERROR)
        << "Failed to inhibit cellular for refreshing SM-DX profiles";
    auto current_request = std::move(remaining_install_requests_.front());

    if (current_request->activation_code.type() ==
        policy_util::SmdxActivationCode::Type::SMDS) {
      CellularNetworkMetricsLogger::LogSmdsScanResult(
          current_request->activation_code.value(),
          /*result=*/std::nullopt);
    }
    PopRequest();
    ScheduleRetryAndProcessRequests(std::move(current_request),
                                    InstallRetryReason::kInternalError);
    return;
  }

  HermesEuiccClient::Get()->RefreshSmdxProfiles(
      euicc_path, GetCurrentActivationCode().value(),
      /*restore_slot=*/true,
      base::BindOnce(&CellularPolicyHandler::OnRefreshSmdxProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     std::move(new_shill_properties), std::move(inhibit_lock),
                     base::TimeTicks::Now()));
}

void CellularPolicyHandler::OnRefreshSmdxProfiles(
    const dbus::ObjectPath& euicc_path,
    base::Value::Dict new_shill_properties,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    base::TimeTicks start_time,
    HermesResponseStatus status,
    const std::vector<dbus::ObjectPath>& profile_paths) {
  DCHECK(inhibit_lock);

  auto& current_request = remaining_install_requests_.front();

  const bool is_smds =
      remaining_install_requests_.front()->activation_code.type() ==
      policy_util::SmdxActivationCode::Type::SMDS;
  if (is_smds) {
    CellularNetworkMetricsLogger::LogSmdsScanResult(
        current_request->activation_code.value(), status);
    CellularNetworkMetricsLogger::LogSmdsScanProfileCount(
        profile_paths.size(),
        CellularNetworkMetricsLogger::SmdsScanMethod::kViaPolicy);
    CellularNetworkMetricsLogger::LogSmdsScanDuration(
        base::TimeTicks::Now() - start_time,
        status == HermesResponseStatus::kSuccess,
        current_request->activation_code.ToString());
  }

  std::unique_ptr<CellularESimProfileWaiter> waiter =
      std::make_unique<CellularESimProfileWaiter>();
  for (const auto& profile_path : profile_paths) {
    waiter->RequirePendingProfile(profile_path);
  }

  auto on_success =
      base::BindOnce(&CellularPolicyHandler::CompleteRefreshSmdxProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     std::move(new_shill_properties), std::move(inhibit_lock),
                     status, profile_paths);
  auto on_shutdown =
      base::BindOnce(&CellularPolicyHandler::PopAndProcessRequests,
                     weak_ptr_factory_.GetWeakPtr());

  waiter->Wait(std::move(on_success), std::move(on_shutdown));

  if (waiter->waiting()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<CellularESimProfileWaiter> waiter) {
              waiter.reset();
            },
            std::move(waiter)),
        kCellularESimProfileWaiterDelay);
  }
}

void CellularPolicyHandler::CompleteRefreshSmdxProfiles(
    const dbus::ObjectPath& euicc_path,
    base::Value::Dict new_shill_properties,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status,
    const std::vector<dbus::ObjectPath>& profile_paths) {
  DCHECK(inhibit_lock);

  auto& current_request = remaining_install_requests_.front();
  const bool is_smds = current_request->activation_code.type() ==
                       policy_util::SmdxActivationCode::Type::SMDS;

  // Scanning SM-DP+ or SM-DS servers will return both a result and available
  // profiles, with the number of profiles that can be returned dependent on
  // which type of server we are scanning. An error being returned indicates
  // there was an issue when performing the scan, but since it does not
  // invalidate the returned profile(s) we simply log the error, capture the
  // error in our metrics, and continue.
  NET_LOG(EVENT) << "HermesEuiccClient::RefreshSmdxProfiles returned with "
                 << "result code " << status << " when called for policy eSIM "
                 << "profile: " << current_request->activation_code.ToString();

  // The activation code will already be known in the SM-DP+ case, but for the
  // sake of using the same flow both both SM-DP+ and SM-DS we choose an
  // activation code from the results in both cases.
  const std::optional<std::string> activation_code =
      GetFirstActivationCode(euicc_path, profile_paths);

  if (!activation_code.has_value()) {
    CellularNetworkMetricsLogger::LogESimPolicyInstallNoAvailableProfiles(
        is_smds
            ? CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmds
            : CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmdp);

    NET_LOG(ERROR) << "Failed to find an available profile that matches the "
                   << "activation code provided by policy: "
                   << current_request->activation_code.ToString();
    auto failed_request = std::move(remaining_install_requests_.front());
    PopRequest();
    // Do not retry if there are no profiles available.
    if (status == HermesResponseStatus::kSuccess) {
      ProcessRequests();
    } else {
      ScheduleRetryAndProcessRequests(
          std::move(failed_request), HermesResponseStatusToRetryReason(status));
    }
    return;
  }

  const bool is_initial_install =
      current_request->retry_backoff.failure_count() == 0;
  if (is_initial_install) {
    CellularNetworkMetricsLogger::LogESimPolicyInstallMethod(
        is_smds
            ? CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmds
            : CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmdp);
  }

  // Confirmation codes are not required when installing policy eSIM profiles
  // using an activation code.
  cellular_esim_installer_->InstallProfileFromActivationCode(
      activation_code.value(),
      /*confirmation_code=*/std::string(), euicc_path,
      std::move(new_shill_properties),
      base::BindOnce(
          &CellularPolicyHandler::OnESimProfileInstallAttemptComplete,
          weak_ptr_factory_.GetWeakPtr()),
      is_initial_install,
      is_smds ? ProfileInstallMethod::kViaSmds
              : ProfileInstallMethod::kViaActivationCodeAfterSmds);
}

void CellularPolicyHandler::OnESimProfileInstallAttemptComplete(
    HermesResponseStatus status,
    std::optional<dbus::ObjectPath> profile_path,
    std::optional<std::string> service_path) {
  DCHECK(is_installing_);

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();

  const bool has_error = status != HermesResponseStatus::kSuccess;
  const bool was_installed = profile_path.has_value();

  if (has_error && !was_installed) {
    if (!base::Contains(kHermesUserErrorCodes, status)) {
      NET_LOG(ERROR)
          << "Failed to install the policy eSIM profile due to a non-user "
          << "error: " << status << ". Scheduling another attempt: "
          << current_request->activation_code.ToString();
      ScheduleRetryAndProcessRequests(
          std::move(current_request),
          HermesResponseStatusToRetryReason(status));
    } else {
      NET_LOG(ERROR)
          << "Failed to install the policy eSIM profile due to a user error: "
          << status << ". Will not schedule another attempt: "
          << current_request->activation_code.ToString();
      ProcessRequests();
    }
    return;
  }

  if (has_error) {
    NET_LOG(ERROR)
        << "Successfully installed policy eSIM profile but failed to "
        << "subsequently enable or connect to the profile: " << status << ". "
        << "Writing the profile information to device prefs and will not "
        << "schedule another attempt: "
        << current_request->activation_code.ToString();
  } else {
    NET_LOG(EVENT) << "Successfully installed policy eSIM profile: "
                   << current_request->activation_code.ToString();
  }

  current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(*profile_path);

  const std::string* name =
      current_request->onc_config.FindString(::onc::network_config::kName);
  DCHECK(name);
  managed_cellular_pref_handler_->AddESimMetadata(
      profile_properties->iccid().value(), *name,
      current_request->activation_code,
      /*sync_stub_networks=*/false);

  managed_network_configuration_handler_->NotifyPolicyAppliedToNetwork(
      *service_path);

  ProcessRequests();
}

void CellularPolicyHandler::OnWaitTimeout() {
  NET_LOG(ERROR) << "Timed out when waiting for the EUICC or profile list.";

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  ScheduleRetryAndProcessRequests(std::move(current_request),
                                  InstallRetryReason::kInternalError);
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

std::optional<std::string> CellularPolicyHandler::GetIccidFromPolicyONC() {
  const std::string* iccid = policy_util::GetIccidFromONC(
      remaining_install_requests_.front()->onc_config);
  if (!iccid || iccid->empty()) {
    return std::nullopt;
  }
  return *iccid;
}

std::optional<dbus::ObjectPath>
CellularPolicyHandler::FindExistingMatchingESimProfile(
    const std::string& iccid) {
  for (CellularESimProfile esim_profile :
       cellular_esim_profile_handler_->GetESimProfiles()) {
    if (esim_profile.iccid() == iccid) {
      return esim_profile.path();
    }
  }
  return std::nullopt;
}

bool CellularPolicyHandler::HasNonCellularInternetConnectivity() {
  const NetworkState* default_network =
      network_state_handler_->DefaultNetwork();
  return default_network && default_network->type() != shill::kTypeCellular &&
         default_network->IsOnline();
}

CellularPolicyHandler::InstallRetryReason
CellularPolicyHandler::HermesResponseStatusToRetryReason(
    HermesResponseStatus status) const {
  if (base::Contains(kHermesInternalErrorCodes, status)) {
    return CellularPolicyHandler::InstallRetryReason::kInternalError;
  }
  if (base::Contains(kHermesUserErrorCodes, status)) {
    return CellularPolicyHandler::InstallRetryReason::kUserError;
  }
  return CellularPolicyHandler::InstallRetryReason::kOther;
}

}  // namespace ash
