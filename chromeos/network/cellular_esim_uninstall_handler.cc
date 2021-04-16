// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_uninstall_handler.h"

#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {

namespace {

void OnRemoveStaleShillService(std::string service_path, bool success) {
  if (success) {
    NET_LOG(EVENT)
        << "Successfully removed stale Shill eSIM configuration. service_path="
        << service_path;
  } else {
    NET_LOG(ERROR)
        << "Error removing stale Shill eSIM configuration. service_path="
        << service_path;
  }
}

}  // namespace

CellularESimUninstallHandler::UninstallRequest::UninstallRequest(
    const std::string& iccid,
    const base::Optional<dbus::ObjectPath>& esim_profile_path,
    const base::Optional<dbus::ObjectPath>& euicc_path,
    UninstallRequestCallback callback)
    : iccid(iccid),
      esim_profile_path(esim_profile_path),
      euicc_path(euicc_path),
      callback(std::move(callback)) {}
CellularESimUninstallHandler::UninstallRequest::~UninstallRequest() = default;

CellularESimUninstallHandler::CellularESimUninstallHandler() = default;
CellularESimUninstallHandler::~CellularESimUninstallHandler() {
  cellular_esim_profile_handler_->RemoveObserver(this);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CellularESimUninstallHandler::Init(
    CellularInhibitor* cellular_inhibitor,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler) {
  cellular_inhibitor_ = cellular_inhibitor;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_connection_handler_ = network_connection_handler;
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);
  cellular_esim_profile_handler_->AddObserver(this);
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::UninstallESim(
    const std::string& iccid,
    const dbus::ObjectPath& esim_profile_path,
    const dbus::ObjectPath& euicc_path,
    UninstallRequestCallback callback) {
  uninstall_requests_.push_back(std::make_unique<UninstallRequest>(
      iccid, esim_profile_path, euicc_path, std::move(callback)));
  ProcessUninstallRequest();
}

void CellularESimUninstallHandler::OnESimProfileListUpdated() {
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::NetworkListChanged() {
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::DevicePropertiesUpdated(
    const DeviceState* device_state) {
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::ProcessUninstallRequest() {
  if (uninstall_requests_.empty()) {
    TransitionToUninstallState(UninstallState::kIdle);
    return;
  }

  if (state_ != UninstallState::kIdle) {
    // Additional uninstall requests are queued. Skip processing a new one while
    // another is in progress.
    return;
  }

  curr_request_network_state_ = nullptr;
  NET_LOG(DEBUG) << "Starting Uninstall Request iccid="
                 << uninstall_requests_.front()->iccid;
  TransitionToUninstallState(UninstallState::kDisconnectingNetwork);
}

void CellularESimUninstallHandler::TransitionToUninstallState(
    UninstallState next_state) {
  NET_LOG(DEBUG) << "ESim Profile Uninstaller changing state " << state_
                 << " to " << next_state;
  state_ = next_state;
  switch (state_) {
    case UninstallState::kIdle:
      // Uninstallation has not started. Do nothing.
      break;
    case UninstallState::kDisconnectingNetwork:
      AttemptNetworkDisconnectIfRequired();
      break;
    case UninstallState::kInhibitingShill:
      AttemptShillInhibit();
      break;
    case UninstallState::kRequestingInstalledProfiles:
      AttemptRequestInstalledProfiles();
      break;
    case UninstallState::kDisablingProfile:
      AttemptDisableProfileIfRequired();
      break;
    case UninstallState::kUninstallingProfile:
      AttemptUninstallProfile();
      break;
    case UninstallState::kRemovingShillService:
      AttemptRemoveShillService();
      break;
    case UninstallState::kSuccess:
    case UninstallState::kFailure:
      std::move(uninstall_requests_.front()->callback)
          .Run(state_ == UninstallState::kSuccess);
      uninstall_requests_.pop_front();
      ProcessUninstallRequest();
      break;
  }
}

void CellularESimUninstallHandler::AttemptNetworkDisconnectIfRequired() {
  const std::string& iccid = uninstall_requests_.front()->iccid;

  curr_request_network_state_ = GetNetworkStateForIccid(iccid);
  if (!curr_request_network_state_) {
    NET_LOG(ERROR) << "Unable to get network state for iccid=" << iccid;
    TransitionToUninstallState(UninstallState::kFailure);
    return;
  }

  // If there is no profile path in the request then this is a stale service.
  // Skip directly to configuration removal.
  if (!uninstall_requests_.front()->esim_profile_path) {
    TransitionToUninstallState(UninstallState::kRemovingShillService);
    return;
  }

  if (!curr_request_network_state_->IsNonShillCellularNetwork() &&
      curr_request_network_state_->IsConnectedState()) {
    network_connection_handler_->DisconnectNetwork(
        curr_request_network_state_->path(),
        base::BindOnce(
            &CellularESimUninstallHandler::TransitionToUninstallState,
            weak_ptr_factory_.GetWeakPtr(), UninstallState::kInhibitingShill),
        base::BindOnce(&CellularESimUninstallHandler::OnNetworkHandlerError,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  TransitionToUninstallState(UninstallState::kInhibitingShill);
}

void CellularESimUninstallHandler::AttemptShillInhibit() {
  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRemovingProfile,
      base::BindOnce(&CellularESimUninstallHandler::OnShillInhibit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnShillInhibit(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhbiting shill";
    TransitionToUninstallState(UninstallState::kFailure);
    return;
  }
  // Save lock in the uninstall request so that it will be released when the
  // request is popped.
  uninstall_requests_.front()->inhibit_lock = std::move(inhibit_lock);
  TransitionToUninstallState(UninstallState::kRequestingInstalledProfiles);
}

void CellularESimUninstallHandler::AttemptRequestInstalledProfiles() {
  cellular_esim_profile_handler_->RefreshProfileList(
      *uninstall_requests_.front()->euicc_path,
      base::BindOnce(&CellularESimUninstallHandler::OnRefreshProfileListResult,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(uninstall_requests_.front()->inhibit_lock));
}

void CellularESimUninstallHandler::OnRefreshProfileListResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error refreshing profile list; state=" << state_;
    TransitionToUninstallState(UninstallState::kFailure);
    return;
  }

  uninstall_requests_.front()->inhibit_lock = std::move(inhibit_lock);
  TransitionToUninstallState(UninstallState::kDisablingProfile);
}

void CellularESimUninstallHandler::AttemptDisableProfileIfRequired() {
  const dbus::ObjectPath& esim_profile_path =
      *uninstall_requests_.front()->esim_profile_path;
  HermesProfileClient::Properties* esim_profile_properties =
      HermesProfileClient::Get()->GetProperties(esim_profile_path);

  if (!esim_profile_properties) {
    NET_LOG(ERROR) << "Unable to find esim profile to be uninstalled";
    TransitionToUninstallState(UninstallState::kFailure);
    return;
  }

  if (esim_profile_properties->state().value() !=
      hermes::profile::State::kActive) {
    NET_LOG(DEBUG) << "Profile is not active skipping disable profile state="
                   << esim_profile_properties->state().value();
    TransitionToUninstallState(UninstallState::kUninstallingProfile);
    return;
  }

  HermesProfileClient::Get()->DisableCarrierProfile(
      esim_profile_path,
      base::BindOnce(&CellularESimUninstallHandler::
                         TransitionUninstallStateOnHermesSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     UninstallState::kUninstallingProfile));
}

void CellularESimUninstallHandler::AttemptUninstallProfile() {
  HermesEuiccClient::Get()->UninstallProfile(
      *uninstall_requests_.front()->euicc_path,
      *uninstall_requests_.front()->esim_profile_path,
      base::BindOnce(&CellularESimUninstallHandler::
                         TransitionUninstallStateOnHermesSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     UninstallState::kRemovingShillService));
}

void CellularESimUninstallHandler::AttemptRemoveShillService() {
  DCHECK(curr_request_network_state_);
  // Return success immediately for non-shill eSIM cellular networks since we
  // don't know the actual shill service path. This stub non-shill service will
  // be removed automatically when the eSIM profile list updates.
  if (curr_request_network_state_->IsNonShillCellularNetwork()) {
    TransitionToUninstallState(UninstallState::kSuccess);
    return;
  }

  network_configuration_handler_->RemoveConfiguration(
      curr_request_network_state_->path(), base::nullopt,
      base::BindOnce(&CellularESimUninstallHandler::TransitionToUninstallState,
                     weak_ptr_factory_.GetWeakPtr(), UninstallState::kSuccess),
      base::BindOnce(&CellularESimUninstallHandler::OnNetworkHandlerError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::TransitionUninstallStateOnHermesSuccess(
    UninstallState next_state,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Hermes error on uninstallation state=" << state_
                   << " status=" << static_cast<int>(status);
    TransitionToUninstallState(UninstallState::kFailure);
    return;
  }
  TransitionToUninstallState(next_state);
}

void CellularESimUninstallHandler::TransitionUninstallStateOnSuccessBoolean(
    UninstallState next_state,
    bool success) {
  if (!success) {
    NET_LOG(ERROR) << "Error on uninstallation state=" << state_;
    TransitionToUninstallState(UninstallState::kFailure);
    return;
  }
  TransitionToUninstallState(next_state);
}

void CellularESimUninstallHandler::OnNetworkHandlerError(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << "Network handler error at state " << state_
                 << " error_name=" << error_name;
  TransitionToUninstallState(UninstallState::kFailure);
}

const NetworkState* CellularESimUninstallHandler::GetNetworkStateForIccid(
    const std::string& iccid) {
  for (auto* const network : GetESimCellularNetworks()) {
    if (network->iccid() == iccid) {
      return network;
    }
  }
  return nullptr;
}

void CellularESimUninstallHandler::CheckStaleESimServices() {
  // Find all known eSIM ICCIDs.
  base::flat_set<std::string> esim_iccids;
  std::vector<CellularESimProfile> esim_profiles =
      cellular_esim_profile_handler_->GetESimProfiles();
  for (const CellularESimProfile& esim_profile : esim_profiles) {
    // Skip pending and installing profiles since they can never have
    // a corresponding Shill service.
    if (esim_profile.state() == CellularESimProfile::State::kPending ||
        esim_profile.state() == CellularESimProfile::State::kInstalling) {
      continue;
    }

    esim_iccids.insert(esim_profile.iccid());
  }

  for (const NetworkState* network_state : GetESimCellularNetworks()) {
    if (esim_iccids.contains(network_state->iccid()))
      continue;

    // Skip if an uninstall request is already queued for this service.
    if (HasQueuedRequest(network_state->iccid()))
      continue;

    NET_LOG(DEBUG) << "Queueing removal for stale shill config. iccid="
                   << network_state->iccid()
                   << "network path=" << network_state->path();
    uninstall_requests_.push_back(std::make_unique<UninstallRequest>(
        network_state->iccid(), /*esim_profile_path=*/base::nullopt,
        /*euicc_path=*/base::nullopt,
        base::BindOnce(&OnRemoveStaleShillService, network_state->path())));
  }
  ProcessUninstallRequest();
}

NetworkStateHandler::NetworkStateList
CellularESimUninstallHandler::GetESimCellularNetworks() {
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

bool CellularESimUninstallHandler::HasQueuedRequest(
    const std::string& iccid) const {
  const auto iter = std::find_if(
      uninstall_requests_.begin(), uninstall_requests_.end(),
      [&](const std::unique_ptr<UninstallRequest>& uninstall_request) {
        return uninstall_request->iccid == iccid;
      });
  return iter != uninstall_requests_.end();
}

std::ostream& operator<<(
    std::ostream& stream,
    const CellularESimUninstallHandler::UninstallState& state) {
  switch (state) {
    case CellularESimUninstallHandler::UninstallState::kIdle:
      stream << "[Idle]";
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
    case CellularESimUninstallHandler::UninstallState::kSuccess:
      stream << "[Success]";
      break;
    case CellularESimUninstallHandler::UninstallState::kFailure:
      stream << "[Failure]";
      break;
  }
  return stream;
}

}  // namespace chromeos