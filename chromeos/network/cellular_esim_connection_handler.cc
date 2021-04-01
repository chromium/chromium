// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_connection_handler.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/time/time.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"

namespace chromeos {
namespace {

constexpr base::TimeDelta kWaitingForConnectableTimeout =
    base::TimeDelta::FromSeconds(30);

base::Optional<dbus::ObjectPath> GetEuiccPath(const std::string& eid) {
  const std::vector<dbus::ObjectPath>& euicc_paths =
      HermesManagerClient::Get()->GetAvailableEuiccs();

  for (const auto& euicc_path : euicc_paths) {
    HermesEuiccClient::Properties* euicc_properties =
        HermesEuiccClient::Get()->GetProperties(euicc_path);
    if (euicc_properties && euicc_properties->eid().value() == eid)
      return euicc_path;
  }

  return base::nullopt;
}

base::Optional<dbus::ObjectPath> GetProfilePath(const std::string& eid,
                                                const std::string& iccid) {
  base::Optional<dbus::ObjectPath> euicc_path = GetEuiccPath(eid);
  if (!euicc_path)
    return base::nullopt;

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(*euicc_path);
  if (!euicc_properties)
    return base::nullopt;

  const std::vector<dbus::ObjectPath>& profile_paths =
      euicc_properties->installed_carrier_profiles().value();
  for (const auto& profile_path : profile_paths) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    if (profile_properties && profile_properties->iccid().value() == iccid)
      return profile_path;
  }

  return base::nullopt;
}

}  // namespace

CellularESimConnectionHandler::ConnectionRequestMetadata::
    ConnectionRequestMetadata(
        std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
        const base::Optional<std::string>& service_path,
        const base::Optional<dbus::ObjectPath>& euicc_path,
        const base::Optional<dbus::ObjectPath>& profile_path,
        CellularESimConnectionHandler::SuccessCallback success_callback,
        network_handler::ErrorCallback error_callback)
    : inhibit_lock(std::move(inhibit_lock)),
      service_path(service_path),
      euicc_path(euicc_path),
      profile_path(profile_path),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

CellularESimConnectionHandler::ConnectionRequestMetadata::
    ~ConnectionRequestMetadata() = default;

CellularESimConnectionHandler::CellularESimConnectionHandler() = default;

CellularESimConnectionHandler::~CellularESimConnectionHandler() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CellularESimConnectionHandler::Init(
    NetworkStateHandler* network_state_handler,
    CellularInhibitor* cellular_inhibitor,
    CellularESimProfileHandler* cellular_esim_profile_handler) {
  network_state_handler_ = network_state_handler;
  cellular_inhibitor_ = cellular_inhibitor;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;

  network_state_handler_->AddObserver(this, FROM_HERE);
}

void CellularESimConnectionHandler::EnableProfileForConnection(
    const std::string& service_path,
    CellularESimConnectionHandler::SuccessCallback success_callback,
    network_handler::ErrorCallback error_callback) {
  request_queue_.emplace(std::make_unique<ConnectionRequestMetadata>(
      /*inhibit_lock=*/nullptr, service_path, /*euicc_path=*/base::nullopt,
      /*profile_path=*/base::nullopt, std::move(success_callback),
      std::move(error_callback)));
  ProcessRequestQueue();
}

void CellularESimConnectionHandler::EnableNewProfileForConnection(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    CellularESimConnectionHandler::SuccessCallback success_callback,
    network_handler::ErrorCallback error_callback) {
  request_queue_.emplace(std::make_unique<ConnectionRequestMetadata>(
      std::move(inhibit_lock), /*service_path=*/base::nullopt, euicc_path,
      profile_path, std::move(success_callback), std::move(error_callback)));
  ProcessRequestQueue();
}

void CellularESimConnectionHandler::NetworkListChanged() {
  if (state_ == ConnectionState::kWaitingForConnectable)
    CheckForConnectable();
}

void CellularESimConnectionHandler::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (state_ == ConnectionState::kWaitingForConnectable)
    CheckForConnectable();
}

void CellularESimConnectionHandler::ProcessRequestQueue() {
  // No requests to process.
  if (request_queue_.empty())
    return;

  // A request is already being processed; wait until that one is finished
  // before processing any additional requests.
  if (state_ != ConnectionState::kIdle)
    return;

  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();
  if (!current_request->service_path) {
    // If the service path is not set. Then this is a newly installed profile
    // being enabled for connection. We will not have a network state for this
    // profile until after it's enabled.  Skip directly to enabling.
    DCHECK(current_request->inhibit_lock);
    DCHECK(current_request->euicc_path);
    DCHECK(current_request->profile_path);
    TransitionToConnectionState(ConnectionState::kEnablingProfile);
    EnableProfile();
  } else {
    TransitionToConnectionState(ConnectionState::kCheckingServiceStatus);
    CheckServiceStatus();
  }
}

void CellularESimConnectionHandler::TransitionToConnectionState(
    ConnectionState state) {
  NET_LOG(DEBUG) << "eSIM connection handler: " << state_ << " => " << state;
  state_ = state;
}

void CellularESimConnectionHandler::CompleteConnectionAttempt(
    const base::Optional<std::string>& error_name,
    const base::Optional<std::string>& service_path) {
  DCHECK(state_ != ConnectionState::kIdle);
  DCHECK(!request_queue_.empty());

  if (timer_.IsRunning())
    timer_.Stop();

  TransitionToConnectionState(ConnectionState::kIdle);
  std::unique_ptr<ConnectionRequestMetadata> metadata =
      std::move(request_queue_.front());
  request_queue_.pop();

  if (error_name) {
    std::move(metadata->error_callback)
        .Run(*error_name,
             /*error_data=*/nullptr);
  } else if (service_path) {
    std::move(metadata->success_callback).Run(*service_path);
  }

  ProcessRequestQueue();

  // In case of errors, metadata will be destroyed at this point along with
  // it's inhibit_lock and the cellular device will uninhibit automatically.
}

const NetworkState*
CellularESimConnectionHandler::GetNetworkStateForCurrentOperation() const {
  if (request_queue_.empty())
    return nullptr;

  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();
  if (current_request->service_path) {
    return network_state_handler_->GetNetworkState(
        *current_request->service_path);
  }

  // If current request has no service path but only profile path then find
  // network using iccid of profile with given profile_path.
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(*current_request->profile_path);
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);
  for (const NetworkState* network : network_list) {
    if (network->iccid() == properties->iccid().value()) {
      return network;
    }
  }
  return nullptr;
}

base::Optional<dbus::ObjectPath>
CellularESimConnectionHandler::GetEuiccPathForCurrentOperation() const {
  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();
  if (current_request->euicc_path) {
    return current_request->euicc_path;
  }

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state)
    return base::nullopt;

  return GetEuiccPath(network_state->eid());
}

base::Optional<dbus::ObjectPath>
CellularESimConnectionHandler::GetProfilePathForCurrentOperation() const {
  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();
  if (current_request->profile_path) {
    return current_request->profile_path;
  }

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state)
    return base::nullopt;

  return GetProfilePath(network_state->eid(), network_state->iccid());
}

void CellularESimConnectionHandler::CheckServiceStatus() {
  DCHECK_EQ(state_, ConnectionState::kCheckingServiceStatus);
  NET_LOG(USER) << "Starting eSIM connection flow for path "
                << *request_queue_.front()->service_path;

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state) {
    NET_LOG(ERROR) << "eSIM connection flow failed to find service";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorNotFound,
                              /*service_path=*/base::nullopt);
    return;
  }

  if (network_state->connectable()) {
    NET_LOG(DEBUG) << "eSIM service is already connectable";
    CompleteConnectionAttempt(/*error_name=*/base::nullopt,
                              network_state->path());
    return;
  }

  TransitionToConnectionState(ConnectionState::kInhibitingScans);
  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kConnectingToProfile,
      base::BindOnce(&CellularESimConnectionHandler::OnInhibitScanResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimConnectionHandler::OnInhibitScanResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(state_, ConnectionState::kInhibitingScans);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "eSIM connection flow failed to inhibit scan";
    CompleteConnectionAttempt(
        NetworkConnectionHandler::kErrorCellularInhibitFailure,
        /*service_path=*/base::nullopt);
    return;
  }

  request_queue_.front()->inhibit_lock = std::move(inhibit_lock);
  TransitionToConnectionState(
      ConnectionState::kRequestingProfilesBeforeEnabling);
  RequestInstalledProfiles();
}

void CellularESimConnectionHandler::RequestInstalledProfiles() {
  base::Optional<dbus::ObjectPath> euicc_path =
      GetEuiccPathForCurrentOperation();
  if (!euicc_path) {
    NET_LOG(ERROR) << "eSIM connection flow could not find relevant EUICC";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue,
                              /*service_path=*/base::nullopt);
    return;
  }

  cellular_esim_profile_handler_->RefreshProfileList(
      *euicc_path,
      base::BindOnce(&CellularESimConnectionHandler::OnRefreshProfileListResult,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(request_queue_.front()->inhibit_lock));
}

void CellularESimConnectionHandler::OnRefreshProfileListResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK(state_ == ConnectionState::kRequestingProfilesBeforeEnabling ||
         state_ == ConnectionState::kRequestingProfilesAfterEnabling);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "eSIM connection flow failed to request profiles";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue,
                              /*service_path=*/base::nullopt);
    return;
  }

  request_queue_.front()->inhibit_lock = std::move(inhibit_lock);

  if (state_ == ConnectionState::kRequestingProfilesAfterEnabling) {
    // Reset the inhibit_lock so that the device will be uninhibited
    // automatically.
    request_queue_.front()->inhibit_lock.reset();
    TransitionToConnectionState(ConnectionState::kWaitingForConnectable);
    CheckForConnectable();
    return;
  }

  TransitionToConnectionState(ConnectionState::kEnablingProfile);
  EnableProfile();
}

void CellularESimConnectionHandler::EnableProfile() {
  base::Optional<dbus::ObjectPath> profile_path =
      GetProfilePathForCurrentOperation();
  if (!profile_path) {
    NET_LOG(ERROR) << "eSIM connection flow could not find profile";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue,
                              /*service_path=*/base::nullopt);
    return;
  }

  HermesProfileClient::Get()->EnableCarrierProfile(
      *profile_path,
      base::BindOnce(
          &CellularESimConnectionHandler::OnEnableCarrierProfileResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimConnectionHandler::OnEnableCarrierProfileResult(
    HermesResponseStatus status) {
  DCHECK_EQ(state_, ConnectionState::kEnablingProfile);

  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "eSIM connection flow failed to enable profile";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue,
                              /*service_path=*/base::nullopt);
    return;
  }

  // Hermes currently exposes stale data after EnableCarrierProfile() completes.
  // To work around this issue, we request the installed profiles one more time.
  // TODO(b/178817914): Remove once underylying issue is fixed.
  TransitionToConnectionState(
      ConnectionState::kRequestingProfilesAfterEnabling);
  RequestInstalledProfiles();
}

void CellularESimConnectionHandler::CheckForConnectable() {
  DCHECK_EQ(state_, ConnectionState::kWaitingForConnectable);
  const NetworkState* network_state = GetNetworkStateForCurrentOperation();

  if (network_state && network_state->connectable()) {
    CompleteConnectionAttempt(/*error_name=*/base::nullopt,
                              network_state->path());
    return;
  }

  // If there is no network state but a service_path was specified then we are
  // handling an existing service and the network disappeared due to some
  // error.
  if (!network_state && request_queue_.front()->service_path) {
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorNotFound,
                              /*service_path=*/base::nullopt);
    return;
  }

  // If network is not connectable or if network state is not available for a
  // newly installed profile, start a timer and wait for the network to become
  // available and connectable.
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, kWaitingForConnectableTimeout,
        base::BindOnce(
            &CellularESimConnectionHandler::OnWaitForConnectableTimeout,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void CellularESimConnectionHandler::OnWaitForConnectableTimeout() {
  DCHECK_EQ(state_, ConnectionState::kWaitingForConnectable);
  NET_LOG(ERROR) << "eSIM connection timed out waiting for network to become "
                 << "connectable";
  CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue,
                            /*service_path=*/base::nullopt);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CellularESimConnectionHandler::ConnectionState& state) {
  switch (state) {
    case CellularESimConnectionHandler::ConnectionState::kIdle:
      stream << "[Idle]";
      break;
    case CellularESimConnectionHandler::ConnectionState::kCheckingServiceStatus:
      stream << "[Checking service status]";
      break;
    case CellularESimConnectionHandler::ConnectionState::kInhibitingScans:
      stream << "[Inhibiting scans]";
      break;
    case CellularESimConnectionHandler::ConnectionState::
        kRequestingProfilesBeforeEnabling:
      stream << "[Requesting profiles before enabling]";
      break;
    case CellularESimConnectionHandler::ConnectionState::kEnablingProfile:
      stream << "[Enabling profile]";
      break;
    case CellularESimConnectionHandler::ConnectionState::
        kRequestingProfilesAfterEnabling:
      stream << "[Requesting profiles after enabling]";
      break;
    case CellularESimConnectionHandler::ConnectionState::kWaitingForConnectable:
      stream << "[Waiting for network to become connectable]";
      break;
  }
  return stream;
}

}  // namespace chromeos
