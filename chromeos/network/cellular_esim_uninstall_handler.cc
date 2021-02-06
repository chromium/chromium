// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_uninstall_handler.h"

#include "base/optional.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {

CellularESimUninstallHandler::UninstallRequest::UninstallRequest(
    const std::string& iccid,
    const dbus::ObjectPath& esim_profile_path,
    const dbus::ObjectPath& euicc_path,
    UninstallRequestCallback callback)
    : iccid(iccid),
      esim_profile_path(esim_profile_path),
      euicc_path(euicc_path),
      callback(std::move(callback)) {}
CellularESimUninstallHandler::UninstallRequest::~UninstallRequest() = default;

CellularESimUninstallHandler::CellularESimUninstallHandler() = default;
CellularESimUninstallHandler::~CellularESimUninstallHandler() = default;

void CellularESimUninstallHandler::Init(
    CellularInhibitor* cellular_inhibitor,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler) {
  cellular_inhibitor_ = cellular_inhibitor;
  network_configuration_handler_ = network_configuration_handler;
  network_connection_handler_ = network_connection_handler;
  network_state_handler_ = network_state_handler;
}

void CellularESimUninstallHandler::UninstallESim(
    const std::string& iccid,
    const dbus::ObjectPath& esim_profile_path,
    const dbus::ObjectPath& euicc_path,
    UninstallRequestCallback callback) {
  uninstall_requests_.emplace(std::make_unique<UninstallRequest>(
      iccid, esim_profile_path, euicc_path, std::move(callback)));
  ProcessUninstallRequest();
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
  NET_LOG(DEBUG) << "Starting Uninstall Request profile_path="
                 << uninstall_requests_.front()->esim_profile_path.value();
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
      uninstall_requests_.pop();
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

  if (curr_request_network_state_->IsConnectedState()) {
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
  HermesEuiccClient::Get()->RequestInstalledProfiles(
      uninstall_requests_.front()->euicc_path,
      base::BindOnce(&CellularESimUninstallHandler::
                         TransitionUninstallStateOnHermesSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     UninstallState::kDisablingProfile));
}

void CellularESimUninstallHandler::AttemptDisableProfileIfRequired() {
  const dbus::ObjectPath& esim_profile_path =
      uninstall_requests_.front()->esim_profile_path;
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
      uninstall_requests_.front()->euicc_path,
      uninstall_requests_.front()->esim_profile_path,
      base::BindOnce(&CellularESimUninstallHandler::
                         TransitionUninstallStateOnHermesSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     UninstallState::kRemovingShillService));
}

void CellularESimUninstallHandler::AttemptRemoveShillService() {
  DCHECK(curr_request_network_state_);
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
  NetworkStateHandler::NetworkStateList cellular_networks;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &cellular_networks);
  for (auto* const network : cellular_networks) {
    if (network->iccid() == iccid) {
      return network;
    }
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