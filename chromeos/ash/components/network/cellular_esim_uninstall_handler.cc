// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_uninstall_handler.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

// static
const base::TimeDelta CellularESimUninstallHandler::kNetworkListWaitTimeout =
    base::Seconds(20);

CellularESimUninstallHandler::UninstallRequest::UninstallRequest(
    const std::optional<std::string>& iccid,
    const std::optional<dbus::ObjectPath>& esim_profile_path,
    const std::optional<dbus::ObjectPath>& euicc_path,
    bool reset_euicc,
    UninstallRequestCallback callback)
    : iccid(iccid),
      esim_profile_path(esim_profile_path),
      euicc_path(euicc_path),
      reset_euicc(reset_euicc),
      callback(std::move(callback)) {}
CellularESimUninstallHandler::UninstallRequest::~UninstallRequest() = default;

CellularESimUninstallHandler::CellularESimUninstallHandler() = default;
CellularESimUninstallHandler::~CellularESimUninstallHandler() {
  OnShuttingDown();
}

void CellularESimUninstallHandler::Init(
    CellularInhibitor* cellular_inhibitor,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler) {
  cellular_inhibitor_ = cellular_inhibitor;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  managed_cellular_pref_handler_ = managed_cellular_pref_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_connection_handler_ = network_connection_handler;
  network_state_handler_ = network_state_handler;

  network_state_handler_observer_.Observe(network_state_handler_.get());
}

void CellularESimUninstallHandler::UninstallESim(
    const std::string& iccid,
    const dbus::ObjectPath& esim_profile_path,
    const dbus::ObjectPath& euicc_path,
    UninstallRequestCallback callback) {
  uninstall_requests_.push_back(std::make_unique<UninstallRequest>(
      iccid, esim_profile_path, euicc_path, /*reset_euicc=*/false,
      std::move(callback)));
  ProcessPendingUninstallRequests();
}

void CellularESimUninstallHandler::ResetEuiccMemory(
    const dbus::ObjectPath& euicc_path,
    UninstallRequestCallback callback) {
  uninstall_requests_.push_back(std::make_unique<UninstallRequest>(
      /*iccid=*/std::nullopt, /*esim_profile_path=*/std::nullopt, euicc_path,
      /*reset_euicc=*/true, std::move(callback)));
  ProcessPendingUninstallRequests();
}

void CellularESimUninstallHandler::NetworkListChanged() {
  if (state_ != UninstallState::kWaitingForNetworkListUpdate) {
    return;
  }
  // When removing multiple eSIM network services back to back after a Reset
  // EUICC, uninstall handler will be in waiting state till next network list
  // update before removing next configuration.
  network_list_wait_timer_.Stop();
  TransitionToUninstallState(UninstallState::kRemovingShillService);
  AttemptRemoveShillService();
}

void CellularESimUninstallHandler::OnShuttingDown() {
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
}

void CellularESimUninstallHandler::ProcessPendingUninstallRequests() {
  // No requests to process.
  if (uninstall_requests_.empty())
    return;

  // Another uninstall request is in progress. Do not process a new request
  // until the previous one has completed
  if (state_ != UninstallState::kIdle)
    return;

  NET_LOG(EVENT) << "Starting eSIM uninstall for request "
                 << *uninstall_requests_.front();
  TransitionToUninstallState(UninstallState::kCheckingNetworkState);
  CheckActiveNetworkState();
}

void CellularESimUninstallHandler::TransitionToUninstallState(
    UninstallState next_state) {
  NET_LOG(EVENT) << "CellularESimUninstallHandler state: " << state_ << " => "
                 << next_state;
  state_ = next_state;
}

void CellularESimUninstallHandler::CompleteCurrentRequest(
    UninstallESimResult result) {
  DCHECK(state_ != UninstallState::kIdle);

  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.UninstallProfile.OperationResult", result);

  const bool success = result == UninstallESimResult::kSuccess;
  NET_LOG(EVENT) << "Completed uninstall request for "
                 << *uninstall_requests_.front() << ". Success = " << success;
  std::move(uninstall_requests_.front()->callback).Run(success);
  last_service_count_removal_for_testing_ =
      uninstall_requests_.front()->removed_service_paths.size();
  uninstall_requests_.pop_front();

  TransitionToUninstallState(UninstallState::kIdle);
  ProcessPendingUninstallRequests();
}

const NetworkState*
CellularESimUninstallHandler::GetNetworkStateForCurrentRequest() const {
  std::optional<std::string> current_request_iccid =
      uninstall_requests_.front()->iccid;

  if (!current_request_iccid) {
    return nullptr;
  }

  for (auto* const network : GetESimCellularNetworks()) {
    if (network->iccid() == current_request_iccid) {
      return network;
    }
  }

  return nullptr;
}

void CellularESimUninstallHandler::CheckActiveNetworkState() {
  DCHECK_EQ(state_, UninstallState::kCheckingNetworkState);

  const NetworkState* network = network_state_handler_->ActiveNetworkByType(
      NetworkTypePattern::Cellular());

  // If the network is connected, disconnect it before we attempt to uninstall
  // eSIM profiles.
  if (network && network->IsConnectedState()) {
    TransitionToUninstallState(UninstallState::kDisconnectingNetwork);
    AttemptNetworkDisconnect(network);
    return;
  }

  TransitionToUninstallState(UninstallState::kInhibitingShill);
  AttemptShillInhibit();
}

void CellularESimUninstallHandler::AttemptNetworkDisconnect(
    const NetworkState* network) {
  DCHECK_EQ(state_, UninstallState::kDisconnectingNetwork);

  network_connection_handler_->DisconnectNetwork(
      network->path(),
      base::BindOnce(&CellularESimUninstallHandler::OnDisconnectSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CellularESimUninstallHandler::OnDisconnectFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnDisconnectSuccess() {
  DCHECK_EQ(state_, UninstallState::kDisconnectingNetwork);

  TransitionToUninstallState(UninstallState::kInhibitingShill);
  AttemptShillInhibit();
}

void CellularESimUninstallHandler::OnDisconnectFailure(
    const std::string& error_name) {
  DCHECK_EQ(state_, UninstallState::kDisconnectingNetwork);

  NET_LOG(ERROR) << "Failed disconnecting network for request "
                 << *uninstall_requests_.front();
  CompleteCurrentRequest(UninstallESimResult::kDisconnectFailed);
}

void CellularESimUninstallHandler::AttemptShillInhibit() {
  DCHECK_EQ(state_, UninstallState::kInhibitingShill);

  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRemovingProfile,
      base::BindOnce(&CellularESimUninstallHandler::OnShillInhibit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnShillInhibit(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(state_, UninstallState::kInhibitingShill);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhbiting Shill during uninstall for request "
                   << *uninstall_requests_.front();
    CompleteCurrentRequest(UninstallESimResult::kInhibitFailed);
    return;
  }

  // Save lock in the uninstall request so that it will be released when the
  // request is popped.
  uninstall_requests_.front()->inhibit_lock = std::move(inhibit_lock);

  TransitionToUninstallState(UninstallState::kRequestingInstalledProfiles);
  AttemptRequestInstalledProfiles();
}

void CellularESimUninstallHandler::AttemptRequestInstalledProfiles() {
  DCHECK_EQ(state_, UninstallState::kRequestingInstalledProfiles);

  cellular_esim_profile_handler_->RefreshProfileList(
      *uninstall_requests_.front()->euicc_path,
      base::BindOnce(&CellularESimUninstallHandler::OnRefreshProfileListResult,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(uninstall_requests_.front()->inhibit_lock));
}

void CellularESimUninstallHandler::OnRefreshProfileListResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(state_, UninstallState::kRequestingInstalledProfiles);

  if (!inhibit_lock) {
    NET_LOG(ERROR)
        << "Error refreshing profile list during uninstall for request "
        << *uninstall_requests_.front();
    CompleteCurrentRequest(UninstallESimResult::kRefreshProfilesFailed);
    return;
  }

  // Save lock back to the uninstall request since we will continue to perform
  // additional eSIM operations.
  uninstall_requests_.front()->inhibit_lock = std::move(inhibit_lock);

  TransitionToUninstallState(UninstallState::kDisablingProfile);
  AttemptDisableProfile();
}

void CellularESimUninstallHandler::AttemptDisableProfile() {
  DCHECK_EQ(state_, UninstallState::kDisablingProfile);
  std::optional<dbus::ObjectPath> esim_profile_path;
  if (uninstall_requests_.front()->reset_euicc) {
    esim_profile_path = GetEnabledCellularESimProfilePath();
    // Skip disabling profile if there are no enabled profiles.
    if (!esim_profile_path) {
      TransitionToUninstallState(UninstallState::kUninstallingProfile);
      AttemptUninstallProfile();
      return;
    }
  } else {
    esim_profile_path = uninstall_requests_.front()->esim_profile_path;
  }
  HermesProfileClient::Get()->DisableCarrierProfile(
      *esim_profile_path,
      base::BindOnce(&CellularESimUninstallHandler::OnDisableProfile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnDisableProfile(
    HermesResponseStatus status) {
  DCHECK_EQ(state_, UninstallState::kDisablingProfile);

  hermes_metrics::LogDisableProfileResult(status);

  bool success = status == HermesResponseStatus::kSuccess ||
                 status == HermesResponseStatus::kErrorAlreadyDisabled;
  if (!success) {
    NET_LOG(ERROR) << "Failed to disable profile for request "
                   << *uninstall_requests_.front();
    CompleteCurrentRequest(UninstallESimResult::kDisableProfileFailed);
    return;
  }

  TransitionToUninstallState(UninstallState::kUninstallingProfile);
  AttemptUninstallProfile();
}

void CellularESimUninstallHandler::AttemptUninstallProfile() {
  DCHECK_EQ(state_, UninstallState::kUninstallingProfile);

  if (uninstall_requests_.front()->reset_euicc) {
    base::flat_set<std::string> iccids =
        GetAllIccidsOnEuicc(*uninstall_requests_.front()->euicc_path);
    HermesEuiccClient::Get()->ResetMemory(
        *uninstall_requests_.front()->euicc_path,
        hermes::euicc::ResetOptions::kDeleteOperationalProfiles,
        base::BindOnce(&CellularESimUninstallHandler::OnUninstallProfile,
                       weak_ptr_factory_.GetWeakPtr(), iccids));
    return;
  }

  base::flat_set<std::string> iccids{*uninstall_requests_.front()->iccid};
  HermesEuiccClient::Get()->UninstallProfile(
      *uninstall_requests_.front()->euicc_path,
      *uninstall_requests_.front()->esim_profile_path,
      base::BindOnce(&CellularESimUninstallHandler::OnUninstallProfile,
                     weak_ptr_factory_.GetWeakPtr(), iccids));
}

void CellularESimUninstallHandler::OnUninstallProfile(
    const base::flat_set<std::string>& removed_iccids,
    HermesResponseStatus status) {
  DCHECK_EQ(state_, UninstallState::kUninstallingProfile);

  if (!uninstall_requests_.front()->reset_euicc) {
    hermes_metrics::LogUninstallProfileResult(status);
  }

  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Failed to uninstall profile for request "
                   << *uninstall_requests_.front();
    CompleteCurrentRequest(UninstallESimResult::kUninstallProfileFailed);
    return;
  }

  if (managed_cellular_pref_handler_) {
    for (const auto& iccid : removed_iccids) {
      managed_cellular_pref_handler_->RemoveESimMetadata(iccid);
    }
  }
  TransitionToUninstallState(UninstallState::kRemovingShillService);
  AttemptRemoveShillService();
}

void CellularESimUninstallHandler::AttemptRemoveShillService() {
  DCHECK_EQ(state_, UninstallState::kRemovingShillService);

  const NetworkState* network = nullptr;
  if (uninstall_requests_.front()->reset_euicc) {
    network = GetNextResetServiceToRemove();
    if (!network) {
      CompleteCurrentRequest(UninstallESimResult::kSuccess);
      return;
    }
  } else {
    network = GetNetworkStateForCurrentRequest();
    if (!network) {
      NET_LOG(ERROR) << "Unable to find eSIM network for request "
                     << *uninstall_requests_.front();
      CompleteCurrentRequest(UninstallESimResult::kRemoveServiceFailed);
      return;
    }

    // Return success immediately for non-shill eSIM cellular networks since we
    // don't know the actual shill service path. This stub non-shill service
    // will be removed automatically when the eSIM profile list updates.
    if (network->IsNonShillCellularNetwork()) {
      CompleteCurrentRequest(UninstallESimResult::kSuccess);
      return;
    }
  }

  NET_LOG(EVENT) << "Attempting to remove Shill service for network: "
                 << network->path();
  network_configuration_handler_->RemoveConfiguration(
      network->path(), std::nullopt,
      base::BindOnce(&CellularESimUninstallHandler::OnRemoveServiceSuccess,
                     weak_ptr_factory_.GetWeakPtr(), network->path()),
      base::BindOnce(&CellularESimUninstallHandler::OnRemoveServiceFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnRemoveServiceSuccess(
    const std::string& removed_service_path) {
  DCHECK_EQ(state_, UninstallState::kRemovingShillService);
  uninstall_requests_.front()->removed_service_paths.insert(
      removed_service_path);

  if (uninstall_requests_.front()->reset_euicc) {
    // Wait for next network list update before removing the next shill service.
    TransitionToUninstallState(UninstallState::kWaitingForNetworkListUpdate);
    network_list_wait_timer_.Start(
        FROM_HERE, kNetworkListWaitTimeout,
        base::BindOnce(&CellularESimUninstallHandler::OnNetworkListWaitTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  CompleteCurrentRequest(UninstallESimResult::kSuccess);
}

void CellularESimUninstallHandler::OnRemoveServiceFailure(
    const std::string& error_name) {
  DCHECK_EQ(state_, UninstallState::kRemovingShillService);
  NET_LOG(ERROR) << "Error removing service for request "
                 << *uninstall_requests_.front() << ". Error: " << error_name;
  CompleteCurrentRequest(UninstallESimResult::kRemoveServiceFailed);
}

void CellularESimUninstallHandler::OnNetworkListWaitTimeout() {
  DCHECK_EQ(state_, UninstallState::kWaitingForNetworkListUpdate);
  NET_LOG(ERROR)
      << "Timedout waiting for network list update after removing service.";
  TransitionToUninstallState(UninstallState::kRemovingShillService);
  AttemptRemoveShillService();
}

NetworkStateHandler::NetworkStateList
CellularESimUninstallHandler::GetESimCellularNetworks() const {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(), /*configured_only=*/false,
      /*visible_only=*/false, /*limit=*/0, &network_list);

  for (auto iter = network_list.begin(); iter != network_list.end();) {
    const NetworkState* network_state = *iter;
    if (network_state->eid().empty()) {
      iter = network_list.erase(iter);
    } else {
      iter++;
    }
  }
  return network_list;
}

std::optional<dbus::ObjectPath>
CellularESimUninstallHandler::GetEnabledCellularESimProfilePath() {
  for (const auto& esim_profile :
       cellular_esim_profile_handler_->GetESimProfiles()) {
    if (esim_profile.state() == CellularESimProfile::State::kActive) {
      return esim_profile.path();
    }
  }
  return std::nullopt;
}

base::flat_set<std::string> CellularESimUninstallHandler::GetAllIccidsOnEuicc(
    const dbus::ObjectPath& euicc_path) {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  const std::string& eid = euicc_properties->eid().value();
  base::flat_set<std::string> iccids;
  for (const auto& esim_profile :
       cellular_esim_profile_handler_->GetESimProfiles()) {
    if (esim_profile.eid() == eid) {
      iccids.emplace(esim_profile.iccid());
    }
  }
  return iccids;
}

const NetworkState* CellularESimUninstallHandler::GetNextResetServiceToRemove()
    const {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          *uninstall_requests_.front()->euicc_path);
  const std::string& eid = euicc_properties->eid().value();
  for (const NetworkState* network : GetESimCellularNetworks()) {
    // Non Shill cellular services cannot be removed. They'll be automatically
    // removed when eSIM profile list updates.
    if (network->IsNonShillCellularNetwork()) {
      continue;
    }

    // b/249825186: When the success callback of
    // NetworkConfigurationHandler::RemoveConfiguration() is called on eSIM
    // cellular shill service(s), the service may be still be exposed as it
    // is still in the process of guaranteed destruction. Chrome relies on Shill
    // pushing an update to the kServiceCompleteList property, which it should
    // eventually, but removing a profile entry is a disk operation so it won't
    // be instantaneous. Successful
    // NetworkConfigurationHandler::RemoveConfiguration() calls to these
    // services should be ignored, as these loadable profile entries no
    // longer exist.
    if (uninstall_requests_.front()->removed_service_paths.contains(
            network->path())) {
      continue;
    }
    if (network->eid() == eid)
      return network;
  }
  return nullptr;
}

std::ostream& operator<<(
    std::ostream& stream,
    const CellularESimUninstallHandler::UninstallState& state) {
  switch (state) {
    case CellularESimUninstallHandler::UninstallState::kIdle:
      stream << "[Idle]";
      break;
    case CellularESimUninstallHandler::UninstallState::kCheckingNetworkState:
      stream << "[Checking network state]";
      break;
    case CellularESimUninstallHandler::UninstallState::kInhibitingShill:
      stream << "[Inhibiting Shill]";
      break;
    case CellularESimUninstallHandler::UninstallState::
        kRequestingInstalledProfiles:
      stream << "[Requesting Installed Profiles]";
      break;
    case CellularESimUninstallHandler::UninstallState::kDisconnectingNetwork:
      stream << "[Disconnecting Network]";
      break;
    case CellularESimUninstallHandler::UninstallState::kDisablingProfile:
      stream << "[Disabling Profile]";
      break;
    case CellularESimUninstallHandler::UninstallState::kUninstallingProfile:
      stream << "[Uninstalling Profile]";
      break;
    case CellularESimUninstallHandler::UninstallState::kRemovingShillService:
      stream << "[Removing Shill Service]";
      break;
    case CellularESimUninstallHandler::UninstallState::
        kWaitingForNetworkListUpdate:
      stream << "[Waiting for network list update]";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const CellularESimUninstallHandler::UninstallRequest& request) {
  if (request.reset_euicc) {
    stream << "(ResetEuicc)";
  } else {
    stream << "(ICCID: " << *request.iccid << ")";
  }
  return stream;
}

}  // namespace ash
