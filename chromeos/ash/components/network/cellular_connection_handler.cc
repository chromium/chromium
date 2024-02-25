// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_connection_handler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {
namespace {

constexpr base::TimeDelta kWaitingForConnectableTimeout = base::Seconds(30);

bool CanInitiateShillConnection(const NetworkState* network) {
  // The network must be part of a Shill profile (i.e., it cannot be a "stub"
  // network which is not exposed by Shill).
  if (network->IsNonProfileType())
    return false;

  // The Connectable property must be set to true, indicating that the network
  // is the active SIM profile in its slot.
  return network->connectable();
}

std::optional<dbus::ObjectPath> GetEuiccPath(const std::string& eid) {
  const std::vector<dbus::ObjectPath>& euicc_paths =
      HermesManagerClient::Get()->GetAvailableEuiccs();

  for (const auto& euicc_path : euicc_paths) {
    HermesEuiccClient::Properties* euicc_properties =
        HermesEuiccClient::Get()->GetProperties(euicc_path);
    if (euicc_properties && euicc_properties->eid().value() == eid)
      return euicc_path;
  }

  return std::nullopt;
}

std::optional<dbus::ObjectPath> GetProfilePath(const std::string& eid,
                                               const std::string& iccid) {
  std::optional<dbus::ObjectPath> euicc_path = GetEuiccPath(eid);
  if (!euicc_path)
    return std::nullopt;

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(*euicc_path);
  if (!euicc_properties)
    return std::nullopt;

  const std::vector<dbus::ObjectPath>& profile_paths =
      euicc_properties->profiles().value();
  for (const auto& profile_path : profile_paths) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    if (profile_properties && profile_properties->iccid().value() == iccid) {
      return profile_path;
    }
  }

  return std::nullopt;
}

}  // namespace

// static
const base::TimeDelta CellularConnectionHandler::kWaitingForAutoConnectTimeout =
    base::Minutes(2);

std::optional<std::string> CellularConnectionHandler::ResultToErrorString(
    PrepareCellularConnectionResult result) {
  switch (result) {
    case PrepareCellularConnectionResult::kSuccess:
      return std::nullopt;

    case PrepareCellularConnectionResult::kCouldNotFindNetworkWithIccid:
      return NetworkConnectionHandler::kErrorNotFound;

    case PrepareCellularConnectionResult::kInhibitFailed:
      return NetworkConnectionHandler::kErrorCellularInhibitFailure;

    case PrepareCellularConnectionResult::kCouldNotFindRelevantEuicc:
      [[fallthrough]];
    case PrepareCellularConnectionResult::kRefreshProfilesFailed:
      [[fallthrough]];
    case PrepareCellularConnectionResult::kCouldNotFindRelevantESimProfile:
      [[fallthrough]];
    case PrepareCellularConnectionResult::kEnableProfileFailed:
      return NetworkConnectionHandler::kErrorESimProfileIssue;

    case PrepareCellularConnectionResult::kTimeoutWaitingForConnectable:
      return NetworkConnectionHandler::kConnectableCellularTimeout;
  }
}

CellularConnectionHandler::ConnectionRequestMetadata::ConnectionRequestMetadata(
    const std::string& iccid,
    SuccessCallback success_callback,
    ErrorCallback error_callback)
    : iccid(iccid),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

CellularConnectionHandler::ConnectionRequestMetadata::ConnectionRequestMetadata(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    SuccessCallback success_callback,
    ErrorCallback error_callback)
    : euicc_path(euicc_path),
      profile_path(profile_path),
      inhibit_lock(std::move(inhibit_lock)),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

CellularConnectionHandler::ConnectionRequestMetadata::
    ~ConnectionRequestMetadata() = default;

CellularConnectionHandler::CellularConnectionHandler() = default;

CellularConnectionHandler::~CellularConnectionHandler() = default;

void CellularConnectionHandler::Init(
    NetworkStateHandler* network_state_handler,
    CellularInhibitor* cellular_inhibitor,
    CellularESimProfileHandler* cellular_esim_profile_handler) {
  network_state_handler_ = network_state_handler;
  cellular_inhibitor_ = cellular_inhibitor;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;

  network_state_handler_observer_.Observe(network_state_handler_.get());
}

void CellularConnectionHandler::PrepareExistingCellularNetworkForConnection(
    const std::string& iccid,
    SuccessCallback success_callback,
    ErrorCallback error_callback) {
  request_queue_.push(std::make_unique<ConnectionRequestMetadata>(
      iccid, std::move(success_callback), std::move(error_callback)));
  ProcessRequestQueue();
}

void CellularConnectionHandler::
    PrepareNewlyInstalledCellularNetworkForConnection(
        const dbus::ObjectPath& euicc_path,
        const dbus::ObjectPath& profile_path,
        std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
        SuccessCallback success_callback,
        ErrorCallback error_callback) {
  request_queue_.push(std::make_unique<ConnectionRequestMetadata>(
      euicc_path, profile_path, std::move(inhibit_lock),
      std::move(success_callback), std::move(error_callback)));
  ProcessRequestQueue();
}

void CellularConnectionHandler::NetworkListChanged() {
  HandleNetworkPropertiesUpdate();
}

void CellularConnectionHandler::NetworkPropertiesUpdated(
    const NetworkState* network) {
  HandleNetworkPropertiesUpdate();
}

void CellularConnectionHandler::NetworkIdentifierTransitioned(
    const std::string& old_service_path,
    const std::string& new_service_path,
    const std::string& old_guid,
    const std::string& new_guid) {
  HandleNetworkPropertiesUpdate();
}

void CellularConnectionHandler::ProcessRequestQueue() {
  // No requests to process.
  if (request_queue_.empty())
    return;

  // A request is already being processed; wait until that one is finished
  // before processing any additional requests.
  if (state_ != ConnectionState::kIdle)
    return;

  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();

  // If the request has an ICCID, it is an existing network. Start by checking
  // the service status.
  if (current_request->iccid) {
    TransitionToConnectionState(ConnectionState::kCheckingServiceStatus);
    CheckServiceStatus();
    return;
  }

  // Otherwise, this is a newly-installed profile, so we can skip straight to
  // enabling it.
  DCHECK(current_request->inhibit_lock);
  DCHECK(current_request->euicc_path);
  DCHECK(current_request->profile_path);
  TransitionToConnectionState(ConnectionState::kEnablingProfile);
  EnableProfile();
}

void CellularConnectionHandler::TransitionToConnectionState(
    ConnectionState state) {
  NET_LOG(EVENT) << "CellularConnectionHandler state: " << state_ << " => "
                 << state;
  state_ = state;
}

void CellularConnectionHandler::CompleteConnectionAttempt(
    PrepareCellularConnectionResult result,
    bool auto_connected) {
  DCHECK(state_ != ConnectionState::kIdle);
  DCHECK(!request_queue_.empty());

  base::UmaHistogramEnumeration(
      "Network.Cellular.PrepareCellularConnection.OperationResult", result);

  if (timer_.IsRunning())
    timer_.Stop();

  std::string service_path;
  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (network_state)
    service_path = network_state->path();

  TransitionToConnectionState(ConnectionState::kIdle);
  std::unique_ptr<ConnectionRequestMetadata> metadata =
      std::move(request_queue_.front());
  request_queue_.pop();

  const std::optional<std::string> error_name = ResultToErrorString(result);

  if (error_name) {
    std::move(metadata->error_callback).Run(service_path, *error_name);
  } else if (service_path.empty()) {
    std::move(metadata->error_callback)
        .Run(service_path, NetworkConnectionHandler::kErrorNotFound);
  } else {
    std::move(metadata->success_callback).Run(service_path, auto_connected);
  }

  ProcessRequestQueue();

  // In case of errors, metadata will be destroyed at this point along with
  // it's inhibit_lock and the cellular device will uninhibit automatically.
}

const NetworkState*
CellularConnectionHandler::GetNetworkStateForCurrentOperation() const {
  if (request_queue_.empty())
    return nullptr;

  std::string iccid;
  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();
  if (current_request->iccid) {
    iccid = *current_request->iccid;
  } else {
    iccid = HermesProfileClient::Get()
                ->GetProperties(*current_request->profile_path)
                ->iccid()
                .value();
  }
  DCHECK(!iccid.empty());

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);
  for (const NetworkState* network : network_list) {
    if (network->iccid() == iccid)
      return network;
  }

  return nullptr;
}

std::optional<dbus::ObjectPath>
CellularConnectionHandler::GetEuiccPathForCurrentOperation() const {
  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();
  if (current_request->euicc_path) {
    return current_request->euicc_path;
  }

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state)
    return std::nullopt;

  return GetEuiccPath(network_state->eid());
}

std::optional<dbus::ObjectPath>
CellularConnectionHandler::GetProfilePathForCurrentOperation() const {
  const ConnectionRequestMetadata* current_request =
      request_queue_.front().get();
  if (current_request->profile_path) {
    return current_request->profile_path;
  }

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state)
    return std::nullopt;

  return GetProfilePath(network_state->eid(), network_state->iccid());
}

void CellularConnectionHandler::CheckServiceStatus() {
  DCHECK_EQ(state_, ConnectionState::kCheckingServiceStatus);

  const std::string& iccid = *request_queue_.front()->iccid;

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (!network_state) {
    NET_LOG(ERROR) << "Could not find network for ICCID "
                   << *request_queue_.front()->iccid;
    CompleteConnectionAttempt(
        PrepareCellularConnectionResult::kCouldNotFindNetworkWithIccid,
        /*auto_connected=*/false);
    return;
  }

  if (CanInitiateShillConnection(network_state)) {
    NET_LOG(USER) << "Cellular service with ICCID " << iccid
                  << " is connectable";
    CompleteConnectionAttempt(PrepareCellularConnectionResult::kSuccess,
                              /*auto_connected=*/false);
    return;
  }

  NET_LOG(USER) << "Starting cellular connection flow. ICCID: " << iccid
                << ", Service path: " << network_state->path()
                << ", EID: " << network_state->eid();

  // If this is a pSIM network, we expect that Shill will eventually expose a
  // connectable Service corresponding to this network. Invoking
  // CheckForConnectable() starts a timeout for this process in case this never
  // ends up occurring.
  if (network_state->eid().empty()) {
    NET_LOG(EVENT) << "Waiting for connectable pSIM network";
    TransitionToConnectionState(ConnectionState::kWaitingForConnectable);
    CheckForConnectable();
    return;
  }

  DCHECK(!request_queue_.front()->inhibit_lock);
  TransitionToConnectionState(ConnectionState::kInhibitingScans);
  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kConnectingToProfile,
      base::BindOnce(&CellularConnectionHandler::OnInhibitScanResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularConnectionHandler::OnInhibitScanResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(state_, ConnectionState::kInhibitingScans);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "eSIM connection flow failed to inhibit scan";
    CompleteConnectionAttempt(PrepareCellularConnectionResult::kInhibitFailed,
                              /*auto_connected=*/false);
    return;
  }

  request_queue_.front()->inhibit_lock = std::move(inhibit_lock);
  TransitionToConnectionState(
      ConnectionState::kRequestingProfilesBeforeEnabling);
  RequestInstalledProfiles();
}

void CellularConnectionHandler::RequestInstalledProfiles() {
  std::optional<dbus::ObjectPath> euicc_path =
      GetEuiccPathForCurrentOperation();
  if (!euicc_path) {
    NET_LOG(ERROR) << "eSIM connection flow could not find relevant EUICC";
    CompleteConnectionAttempt(
        PrepareCellularConnectionResult::kCouldNotFindRelevantEuicc,
        /*auto_connected=*/false);
    return;
  }

  cellular_esim_profile_handler_->RefreshProfileList(
      *euicc_path,
      base::BindOnce(&CellularConnectionHandler::OnRefreshProfileListResult,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(request_queue_.front()->inhibit_lock));
}

void CellularConnectionHandler::OnRefreshProfileListResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(ConnectionState::kRequestingProfilesBeforeEnabling, state_);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "eSIM connection flow failed to request profiles";
    CompleteConnectionAttempt(
        PrepareCellularConnectionResult::kRefreshProfilesFailed,
        /*auto_connected=*/false);
    return;
  }

  request_queue_.front()->inhibit_lock = std::move(inhibit_lock);

  TransitionToConnectionState(ConnectionState::kEnablingProfile);
  EnableProfile();
}

void CellularConnectionHandler::EnableProfile() {
  std::optional<dbus::ObjectPath> profile_path =
      GetProfilePathForCurrentOperation();
  if (!profile_path) {
    NET_LOG(ERROR) << "eSIM connection flow could not find profile";
    CompleteConnectionAttempt(
        PrepareCellularConnectionResult::kCouldNotFindRelevantESimProfile,
        /*auto_connected=*/false);
    return;
  }

  HermesProfileClient::Get()->EnableCarrierProfile(
      *profile_path,
      base::BindOnce(&CellularConnectionHandler::OnEnableCarrierProfileResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularConnectionHandler::OnEnableCarrierProfileResult(
    HermesResponseStatus status) {
  DCHECK_EQ(state_, ConnectionState::kEnablingProfile);

  hermes_metrics::LogEnableProfileResult(status);

  // If we try to enable and "fail" with an already-enabled error, count this as
  // a success.
  bool success = status == HermesResponseStatus::kSuccess ||
                 status == HermesResponseStatus::kErrorAlreadyEnabled;

  if (!success) {
    NET_LOG(ERROR) << "eSIM connection flow failed to enable profile";
    CompleteConnectionAttempt(
        PrepareCellularConnectionResult::kEnableProfileFailed,
        /*auto_connected=*/false);
    return;
  }

  // kErrorAlreadyEnabled implies that the SIM profile was already enabled.
  request_queue_.front()->did_connection_require_enabling_profile =
      status == HermesResponseStatus::kSuccess;

  // Reset the inhibit_lock so that the device will be uninhibited
  // automatically.
  request_queue_.front()->inhibit_lock.reset();
  TransitionToConnectionState(ConnectionState::kWaitingForConnectable);
  CheckForConnectable();
}

void CellularConnectionHandler::HandleNetworkPropertiesUpdate() {
  if (state_ == ConnectionState::kWaitingForConnectable)
    CheckForConnectable();
}

void CellularConnectionHandler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (state_ == ConnectionState::kWaitingForShillAutoConnect) {
    CheckForAutoConnected();
  }
}

void CellularConnectionHandler::CheckForConnectable() {
  DCHECK_EQ(state_, ConnectionState::kWaitingForConnectable);

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (network_state && CanInitiateShillConnection(network_state)) {
    if (!request_queue_.front()->did_connection_require_enabling_profile) {
      CompleteConnectionAttempt(PrepareCellularConnectionResult::kSuccess,
                                /*auto_connected=*/false);
    } else {
      StartWaitingForShillAutoConnect();
    }
    return;
  }

  // If network is not connectable or if network state is not available for a
  // newly installed profile, start a timer and wait for the network to become
  // available and connectable.
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, kWaitingForConnectableTimeout,
        base::BindOnce(&CellularConnectionHandler::OnWaitForConnectableTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CellularConnectionHandler::OnWaitForConnectableTimeout() {
  DCHECK_EQ(state_, ConnectionState::kWaitingForConnectable);
  NET_LOG(ERROR) << "Cellular connection timed out waiting for network to "
                    "become connectable";
  CompleteConnectionAttempt(
      PrepareCellularConnectionResult::kTimeoutWaitingForConnectable,
      /*auto_connected=*/false);
}

void CellularConnectionHandler::StartWaitingForShillAutoConnect() {
  // Stop the timer that wait for the network to become connectable.
  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  TransitionToConnectionState(ConnectionState::kWaitingForShillAutoConnect);
  CheckForAutoConnected();
}

void CellularConnectionHandler::CheckForAutoConnected() {
  CHECK_EQ(state_, ConnectionState::kWaitingForShillAutoConnect);

  const NetworkState* network_state = GetNetworkStateForCurrentOperation();
  if (network_state && network_state->IsConnectedState()) {
    CompleteConnectionAttempt(PrepareCellularConnectionResult::kSuccess,
                              /*auto_connected=*/true);
    return;
  }

  // If network hasn't autoconnected by Shill yet, start a timer and wait for
  // the network to become connected.
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, kWaitingForAutoConnectTimeout,
        base::BindOnce(&CellularConnectionHandler::OnWaitForAutoConnectTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CellularConnectionHandler::OnWaitForAutoConnectTimeout() {
  DCHECK_EQ(state_, ConnectionState::kWaitingForShillAutoConnect);
  NET_LOG(ERROR) << "Cellular connection timed out waiting for network to "
                    "become auto connected";
  CompleteConnectionAttempt(PrepareCellularConnectionResult::kSuccess,
                            /*auto_connected=*/false);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CellularConnectionHandler::ConnectionState& state) {
  switch (state) {
    case CellularConnectionHandler::ConnectionState::kIdle:
      stream << "[Idle]";
      break;
    case CellularConnectionHandler::ConnectionState::kCheckingServiceStatus:
      stream << "[Checking service status]";
      break;
    case CellularConnectionHandler::ConnectionState::kInhibitingScans:
      stream << "[Inhibiting scans]";
      break;
    case CellularConnectionHandler::ConnectionState::
        kRequestingProfilesBeforeEnabling:
      stream << "[Requesting profiles before enabling]";
      break;
    case CellularConnectionHandler::ConnectionState::kEnablingProfile:
      stream << "[Enabling profile]";
      break;
    case CellularConnectionHandler::ConnectionState::kWaitingForConnectable:
      stream << "[Waiting for network to become connectable]";
      break;
    case CellularConnectionHandler::ConnectionState::
        kWaitingForShillAutoConnect:
      stream << "[Waiting for network to become auto-connected]";
      break;
  }
  return stream;
}

}  // namespace ash
