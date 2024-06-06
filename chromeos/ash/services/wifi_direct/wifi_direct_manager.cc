// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/wifi_direct/wifi_direct_manager.h"

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_group.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_metrics_logger.h"
#include "chromeos/ash/services/wifi_direct/wifi_direct_connection.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::wifi_direct {

using wifi_direct::mojom::WifiCredentialsPtr;
using wifi_direct::mojom::WifiDirectOperationResult;

namespace {

WifiDirectOperationResult GetMojoOperationResult(
    WifiP2PController::OperationResult result) {
  switch (result) {
    case WifiP2PController::OperationResult::kSuccess:
      return WifiDirectOperationResult::kSuccess;
    case WifiP2PController::OperationResult::kNotAllowed:
      return WifiDirectOperationResult::kNotAllowed;
    case WifiP2PController::OperationResult::kNotSupported:
      return WifiDirectOperationResult::kNotSupported;
    case WifiP2PController::OperationResult::kNotConnected:
      return WifiDirectOperationResult::kNotConnected;
    case WifiP2PController::OperationResult::kConcurrencyNotSupported:
      return WifiDirectOperationResult::kConcurrencyNotSupported;
    case WifiP2PController::OperationResult::kFrequencyNotSupported:
      return WifiDirectOperationResult::kFrequencyNotSupported;
    case WifiP2PController::OperationResult::kAuthFailure:
      return WifiDirectOperationResult::kAuthFailure;
    case WifiP2PController::OperationResult::kGroupNotFound:
      return WifiDirectOperationResult::kGroupNotFound;
    case WifiP2PController::OperationResult::kAlreadyConnected:
      return WifiDirectOperationResult::kAlreadyConnected;
    case WifiP2PController::OperationResult::kOperationInProgress:
      return WifiDirectOperationResult::kOperationInProgress;
    case WifiP2PController::OperationResult::kInvalidArguments:
      return WifiDirectOperationResult::kInvalidArguments;
    case WifiP2PController::OperationResult::kTimeout:
      return WifiDirectOperationResult::kTimeout;
    case WifiP2PController::OperationResult::kInvalidResultCode:
      return WifiDirectOperationResult::kInvalidResultCode;
    case WifiP2PController::OperationResult::kInvalidGroupProperties:
      return WifiDirectOperationResult::kInvalidGroupProperties;
    case WifiP2PController::OperationResult::kOperationFailed:
    case WifiP2PController::OperationResult::kDBusError:
      return WifiDirectOperationResult::kUnknownFailure;
  }
}

}  // namespace

WifiDirectManager::WifiDirectManager() {
  DCHECK(WifiP2PController::IsInitialized());
  WifiP2PController::Get()->AddObserver(this);
}

WifiDirectManager::~WifiDirectManager() {
  WifiP2PController::Get()->RemoveObserver(this);
}

void WifiDirectManager::BindPendingReceiver(
    mojo::PendingReceiver<mojom::WifiDirectManager> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WifiDirectManager::CreateWifiDirectGroup(
    WifiCredentialsPtr credentials,
    CreateWifiDirectGroupCallback callback) {
  WifiP2PController::Get()->CreateWifiP2PGroup(
      credentials ? std::optional{credentials->ssid} : std::nullopt,
      credentials ? std::optional{credentials->passphrase} : std::nullopt,
      base::BindOnce(&WifiDirectManager::OnCreateOrConnectWifiDirectGroup,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WifiDirectManager::ConnectToWifiDirectGroup(
    WifiCredentialsPtr credentials,
    std::optional<uint32_t> frequency,
    ConnectToWifiDirectGroupCallback callback) {
  CHECK(credentials);
  WifiP2PController::Get()->ConnectToWifiP2PGroup(
      credentials->ssid, credentials->passphrase, frequency,
      base::BindOnce(&WifiDirectManager::OnCreateOrConnectWifiDirectGroup,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WifiDirectManager::GetWifiP2PCapabilities(
    WifiDirectManager::GetWifiP2PCapabilitiesCallback callback) {
  auto result = mojom::WifiP2PCapabilities::New();

  result->is_owner_ready =
      WifiP2PController::Get()->GetP2PCapabilities().is_owner_ready;
  result->is_client_ready =
      WifiP2PController::Get()->GetP2PCapabilities().is_client_ready;
  result->is_p2p_supported =
      WifiP2PController::Get()->GetP2PCapabilities().is_p2p_supported;

  std::move(callback).Run(std::move(result));
}

void WifiDirectManager::OnWifiDirectConnectionDisconnected(const int shill_id,
                                                           bool is_owner) {
  const auto it = shill_id_to_wifi_direct_connection_.find(shill_id);
  if (it == shill_id_to_wifi_direct_connection_.end()) {
    NET_LOG(ERROR) << "Couldn't find the Wifi direct connection with shill id: "
                   << shill_id << " in map";
    return;
  }
  CHECK(shill_id_to_wifi_direct_connection_[shill_id]->IsOwner() == is_owner);
  WifiP2PMetricsLogger::RecordWifiP2PDisconnectReason(
      WifiP2PMetricsLogger::DisconnectReason::kInternalError, is_owner);

  shill_id_to_wifi_direct_connection_[shill_id].reset();
  shill_id_to_wifi_direct_connection_.erase(it);
}

void WifiDirectManager::OnCreateOrConnectWifiDirectGroup(
    CreateWifiDirectGroupCallback callback,
    WifiP2PController::OperationResult result,
    std::optional<WifiP2PGroup> group_metadata) {
  WifiDirectOperationResult mojo_result = GetMojoOperationResult(result);
  if (mojo_result != WifiDirectOperationResult::kSuccess) {
    std::move(callback).Run(mojo_result, mojo::NullRemote());
    return;
  }

  CHECK(group_metadata);
  const int shill_id = group_metadata->shill_id();
  NET_LOG(EVENT) << "Creating Wifi direct connection with Shill id: "
                 << shill_id;

  auto wifi_direct_connection_pair = WifiDirectConnection::Create(
      *group_metadata,
      base::BindOnce(&WifiDirectManager::OnClientRequestedDisconnection,
                     weak_ptr_factory_.GetWeakPtr(), shill_id));
  if (base::Contains(shill_id_to_wifi_direct_connection_, shill_id)) {
    NET_LOG(ERROR) << "Found an existing Wifi direct connection with Shill id: "
                   << shill_id;
  }

  shill_id_to_wifi_direct_connection_.insert_or_assign(
      shill_id, std::move(wifi_direct_connection_pair.first));
  std::move(callback).Run(WifiDirectOperationResult::kSuccess,
                          std::move(wifi_direct_connection_pair.second));
}

void WifiDirectManager::OnDestroyOrDisconnectWifiDirectGroup(
    WifiP2PController::OperationResult result) {
  WifiDirectOperationResult mojo_result = GetMojoOperationResult(result);

  if (mojo_result != WifiDirectOperationResult::kSuccess) {
    NET_LOG(ERROR) << "Disconnect or Destroy operation failed with the code: "
                   << mojo_result;
    return;
  }

  NET_LOG(EVENT)
      << "Successfully disconnected or destroyed the wifi direct group";
}

void WifiDirectManager::OnClientRequestedDisconnection(int shill_id) {
  NET_LOG(EVENT)
      << "Request diconnection for the Wifi direct group with Shill id: "
      << shill_id;
  const auto it = shill_id_to_wifi_direct_connection_.find(shill_id);
  if (it == shill_id_to_wifi_direct_connection_.end()) {
    NET_LOG(ERROR) << "Couldn't find the Wifi direct connection with shill id: "
                   << shill_id << " in map";
    return;
  }
  const bool is_owner =
      shill_id_to_wifi_direct_connection_[shill_id]->IsOwner();
  if (is_owner) {
    WifiP2PController::Get()->DestroyWifiP2PGroup(
        shill_id,
        base::BindOnce(&WifiDirectManager::OnDestroyOrDisconnectWifiDirectGroup,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    WifiP2PController::Get()->DisconnectFromWifiP2PGroup(
        shill_id,
        base::BindOnce(&WifiDirectManager::OnDestroyOrDisconnectWifiDirectGroup,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  WifiP2PMetricsLogger::RecordWifiP2PDisconnectReason(
      WifiP2PMetricsLogger::DisconnectReason::kClientInitiated, is_owner);
  shill_id_to_wifi_direct_connection_.erase(it);
}

size_t WifiDirectManager::GetConnectionsCountForTesting() const {
  return shill_id_to_wifi_direct_connection_.size();
}

void WifiDirectManager::FlushForTesting() {
  receivers_.FlushForTesting();  // IN-TEST
  for (const auto& shill_id_connection_pair :
       shill_id_to_wifi_direct_connection_) {
    shill_id_connection_pair.second->FlushForTesting();  // IN-TEST
  }
}

}  // namespace ash::wifi_direct
