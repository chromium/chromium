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
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

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
    ConnectionRequestMetadata(const std::string& service_path,
                              base::OnceClosure success_callback,
                              network_handler::ErrorCallback error_callback)
    : service_path(service_path),
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
    CellularInhibitor* cellular_inhibitor) {
  network_state_handler_ = network_state_handler;
  cellular_inhibitor_ = cellular_inhibitor;

  network_state_handler_->AddObserver(this, FROM_HERE);
}

void CellularESimConnectionHandler::EnableProfileForConnection(
    const std::string& service_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  request_queue_.emplace(std::make_unique<ConnectionRequestMetadata>(
      service_path, std::move(success_callback), std::move(error_callback)));
  ProcessRequestQueue();
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

  TransitionToConnectionState(ConnectionState::kCheckingServiceStatus);
  CheckServiceStatus();
}

void CellularESimConnectionHandler::TransitionToConnectionState(
    ConnectionState state) {
  NET_LOG(DEBUG) << "eSIM connection handler: " << state_ << " => " << state;
  state_ = state;
}

void CellularESimConnectionHandler::CompleteConnectionAttempt(
    const base::Optional<std::string>& error_name) {
  DCHECK(state_ != ConnectionState::kIdle);
  DCHECK(!request_queue_.empty());

  if (timer_.IsRunning())
    timer_.Stop();

  // If there was an error, but we've already inhibited scans, we need to
  // uninhibit scans before returning a result to ensure that cellular
  // connectivity continues to work.
  if (inhibit_lock_) {
    UninhibitScans(error_name);
    return;
  }

  TransitionToConnectionState(ConnectionState::kIdle);
  std::unique_ptr<ConnectionRequestMetadata> metadata =
      std::move(request_queue_.front());
  request_queue_.pop();

  if (error_name) {
    std::move(metadata->error_callback)
        .Run(*error_name,
             /*error_data=*/nullptr);
  } else {
    std::move(metadata->success_callback).Run();
  }

  ProcessRequestQueue();
}

const NetworkState*
CellularESimConnectionHandler::GetNetworkStateForCurrentOperation() const {
  if (request_queue_.empty())
    return nullptr;

  return network_state_handler_->GetNetworkState(
      request_queue_.front()->service_path);
}

base::Optional<dbus::ObjectPath>
CellularESimConnectionHandler::GetEuiccPathForCurrentOperation() const {
  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state)
    return base::nullopt;

  return GetEuiccPath(network_state->eid());
}

base::Optional<dbus::ObjectPath>
CellularESimConnectionHandler::GetProfilePathForCurrentOperation() const {
  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state)
    return base::nullopt;

  return GetProfilePath(network_state->eid(), network_state->iccid());
}

void CellularESimConnectionHandler::CheckServiceStatus() {
  DCHECK_EQ(state_, ConnectionState::kCheckingServiceStatus);
  NET_LOG(USER) << "Starting eSIM connection flow for path "
                << request_queue_.front()->service_path;

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state) {
    NET_LOG(ERROR) << "eSIM connection flow failed to find service";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  if (network_state->connectable()) {
    NET_LOG(DEBUG) << "eSIM service is already connectable";
    CompleteConnectionAttempt(/*error_name=*/base::nullopt);
    return;
  }

  TransitionToConnectionState(ConnectionState::kInhibitingScans);
  cellular_inhibitor_->InhibitCellularScanning(
      base::BindOnce(&CellularESimConnectionHandler::OnInhibitScanResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimConnectionHandler::OnInhibitScanResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(state_, ConnectionState::kInhibitingScans);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "eSIM connection flow failed to inhibit scan";
    CompleteConnectionAttempt(
        NetworkConnectionHandler::kErrorCellularInhibitFailure);
    return;
  }

  inhibit_lock_ = std::move(inhibit_lock);
  TransitionToConnectionState(
      ConnectionState::kRequestingProfilesBeforeEnabling);
  RequestInstalledProfiles();
}

void CellularESimConnectionHandler::RequestInstalledProfiles() {
  base::Optional<dbus::ObjectPath> euicc_path =
      GetEuiccPathForCurrentOperation();
  if (!euicc_path) {
    NET_LOG(ERROR) << "eSIM connection flow could not find relevant EUICC";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue);
    return;
  }

  HermesEuiccClient::Get()->RequestInstalledProfiles(
      *euicc_path,
      base::BindOnce(
          &CellularESimConnectionHandler::OnRequestInstalledProfilesResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimConnectionHandler::OnRequestInstalledProfilesResult(
    HermesResponseStatus status) {
  DCHECK(state_ == ConnectionState::kRequestingProfilesBeforeEnabling ||
         state_ == ConnectionState::kRequestingProfilesAfterEnabling);

  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "eSIM connection flow failed to request profiles";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue);
    return;
  }

  if (state_ == ConnectionState::kRequestingProfilesAfterEnabling) {
    UninhibitScans(/*error_before_uninhibit=*/base::nullopt);
    return;
  }

  TransitionToConnectionState(ConnectionState::kEnablingProfile);
  base::Optional<dbus::ObjectPath> profile_path =
      GetProfilePathForCurrentOperation();
  if (!profile_path) {
    NET_LOG(ERROR) << "eSIM connection flow could not find profile";
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue);
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
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue);
    return;
  }

  // Hermes currently exposes stale data after EnableCarrierProfile() completes.
  // To work around this issue, we request the installed profiles one more time.
  // TODO(b/178817914): Remove once underylying issue is fixed.
  TransitionToConnectionState(
      ConnectionState::kRequestingProfilesAfterEnabling);
  RequestInstalledProfiles();
}

void CellularESimConnectionHandler::UninhibitScans(
    const base::Optional<std::string>& error_before_uninhibit) {
  DCHECK(inhibit_lock_);

  TransitionToConnectionState(ConnectionState::kUninhibitingScans);
  inhibit_lock_.reset();

  if (error_before_uninhibit) {
    CompleteConnectionAttempt(*error_before_uninhibit);
    return;
  }

  TransitionToConnectionState(ConnectionState::kWaitingForConnectable);
  CheckForConnectable();
}

void CellularESimConnectionHandler::CheckForConnectable() {
  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state) {
    CompleteConnectionAttempt(NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  // If not yet connectable, start a timer and wait for the network to become
  // connectable.
  if (!network_state->connectable()) {
    if (!timer_.IsRunning()) {
      timer_.Start(
          FROM_HERE, kWaitingForConnectableTimeout,
          base::BindOnce(
              &CellularESimConnectionHandler::OnWaitForConnectableTimeout,
              weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  CompleteConnectionAttempt(/*error_name=*/base::nullopt);
}

void CellularESimConnectionHandler::OnWaitForConnectableTimeout() {
  DCHECK_EQ(state_, ConnectionState::kWaitingForConnectable);
  NET_LOG(ERROR) << "eSIM connection timed out waiting for network to become "
                 << "connectable";
  CompleteConnectionAttempt(NetworkConnectionHandler::kErrorESimProfileIssue);
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
    case CellularESimConnectionHandler::ConnectionState::kUninhibitingScans:
      stream << "[Uninhibiting scans]";
      break;
    case CellularESimConnectionHandler::ConnectionState::kWaitingForConnectable:
      stream << "[Waiting for network to become connectable]";
      break;
  }
  return stream;
}

}  // namespace chromeos
