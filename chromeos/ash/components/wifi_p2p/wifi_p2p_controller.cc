// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

WifiP2PController* g_wifi_p2p_controller = nullptr;

WifiP2PController::OperationResult ShillResultToEnum(
    const std::string& shill_result_code) {
  if (shill_result_code == shill::kCreateP2PGroupResultSuccess ||
      shill_result_code == shill::kConnectToP2PGroupResultSuccess) {
    return WifiP2PController::OperationResult::kSuccess;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultNotAllowed ||
      shill_result_code == shill::kConnectToP2PGroupResultNotAllowed) {
    return WifiP2PController::OperationResult::kNotAllowed;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultNotSupported ||
      shill_result_code == shill::kConnectToP2PGroupResultNotSupported) {
    return WifiP2PController::OperationResult::kNotSupported;
  }
  if (shill_result_code ==
          shill::kCreateP2PGroupResultConcurrencyNotSupported ||
      shill_result_code ==
          shill::kConnectToP2PGroupResultConcurrencyNotSupported) {
    return WifiP2PController::OperationResult::kConcurrencyNotSupported;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultTimeout ||
      shill_result_code == shill::kConnectToP2PGroupResultTimeout) {
    return WifiP2PController::OperationResult::kTimeout;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultFrequencyNotSupported ||
      shill_result_code ==
          shill::kConnectToP2PGroupResultFrequencyNotSupported) {
    return WifiP2PController::OperationResult::kFrequencyNotSupported;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultBadSSID ||
      shill_result_code == shill::kCreateP2PGroupResultInvalidArguments ||
      shill_result_code == shill::kConnectToP2PGroupResultInvalidArguments) {
    return WifiP2PController::OperationResult::kInvalidArguments;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultOperationInProgress ||
      shill_result_code == shill::kConnectToP2PGroupResultOperationInProgress) {
    return WifiP2PController::OperationResult::kOperationInProgress;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultOperationFailed ||
      shill_result_code == shill::kConnectToP2PGroupResultOperationFailed) {
    return WifiP2PController::OperationResult::kOperationFailed;
  }
  if (shill_result_code == shill::kConnectToP2PGroupResultAuthFailure) {
    return WifiP2PController::OperationResult::kAuthFailure;
  }
  if (shill_result_code == shill::kConnectToP2PGroupResultGroupNotFound) {
    return WifiP2PController::OperationResult::kGroupNotFound;
  }
  if (shill_result_code == shill::kConnectToP2PGroupResultAlreadyConnected) {
    return WifiP2PController::OperationResult::kAlreadyConnected;
  }

  NET_LOG(ERROR) << "Unexpected result code: " << shill_result_code;
  return WifiP2PController::OperationResult::kUnknownError;
}

}  // namespace

WifiP2PController::WifiP2PController() = default;

WifiP2PController::~WifiP2PController() = default;

void WifiP2PController::Init() {
  ShillManagerClient::Get()->SetProperty(
      shill::kP2PAllowedProperty,
      base::Value(ash::features::IsWifiDirectEnabled()), base::DoNothing(),
      base::BindOnce(&WifiP2PController::OnSetManagerPropertyFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     shill::kP2PAllowedProperty));
}

void WifiP2PController::OnSetManagerPropertyFailure(
    const std::string& property_name,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Error setting Shill manager properties: " << property_name
                 << ", error: " << error_name << ", message: " << error_message;
}

// static
void WifiP2PController::Initialize() {
  CHECK(!g_wifi_p2p_controller);
  g_wifi_p2p_controller = new WifiP2PController();
  g_wifi_p2p_controller->Init();
}

// static
void WifiP2PController::Shutdown() {
  CHECK(g_wifi_p2p_controller);
  delete g_wifi_p2p_controller;
  g_wifi_p2p_controller = nullptr;
}

// static
WifiP2PController* WifiP2PController::Get() {
  CHECK(g_wifi_p2p_controller)
      << "WifiP2PController::Get() called before Initialize()";
  return g_wifi_p2p_controller;
}

// static
bool WifiP2PController::IsInitialized() {
  return g_wifi_p2p_controller;
}

void WifiP2PController::CreateWifiP2PGroup(const std::string& ssid,
                                           const std::string& passphrase,
                                           WifiP2PGroupCallback callback) {
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->CreateP2PGroup(
      ShillManagerClient::CreateP2PGroupParameter{ssid, passphrase},
      base::BindOnce(&WifiP2PController::OnCreateOrConnectP2PGroupSuccess,
                     weak_ptr_factory_.GetWeakPtr(), /*create_group=*/true,
                     std::move(callback_split.first)),
      base::BindOnce(&WifiP2PController::OnCreateOrConnectP2PGroupFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.second)));
}

void WifiP2PController::OnCreateOrConnectP2PGroupSuccess(
    bool create_group,
    WifiP2PGroupCallback callback,
    base::Value::Dict result_dict) {
  NET_LOG(EVENT) << "CreateOrConnectP2PGroup operation succeeded with result: "
                 << result_dict;

  const std::string* result_code =
      result_dict.FindString(shill::kP2PResultCode);
  if (!result_code) {
    std::move(callback).Run(OperationResult::kInvalidResultCode,
                            /*metadata=*/std::nullopt);
    return;
  }

  const OperationResult result = ShillResultToEnum(*result_code);
  if (result != OperationResult::kSuccess) {
    std::move(callback).Run(result, /*metadata=*/std::nullopt);
    return;
  }

  std::optional<int> shill_id = result_dict.FindInt(shill::kP2PDeviceShillID);
  if (!shill_id) {
    NET_LOG(ERROR) << "Missing shill_id in Wifi direct operation response when "
                      "result code is success";
    std::move(callback).Run(OperationResult::kInvalidResultCode,
                            /*metadata=*/std::nullopt);
    return;
  }

  ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &WifiP2PController::GetP2PGroupMetadata, weak_ptr_factory_.GetWeakPtr(),
      *shill_id, create_group, std::move(callback)));
}

void WifiP2PController::GetP2PGroupMetadata(
    int shill_id,
    bool is_owner,
    WifiP2PGroupCallback callback,
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "Error getting Shill manager properties.";
    std::move(callback).Run(OperationResult::kInvalidGroupProperties,
                            /*metadata=*/std::nullopt);
    return;
  }

  base::Value::List* entry_list =
      properties->FindList(is_owner ? shill::kP2PGroupInfosProperty
                                    : shill::kP2PClientInfosProperty);
  if (!entry_list || entry_list->size() == 0) {
    std::move(callback).Run(OperationResult::kInvalidGroupProperties,
                            /*metadata=*/std::nullopt);
    return;
  }

  if (entry_list->size() > 1) {
    NET_LOG(ERROR) << "Found more than one P2P group info.";
  }

  for (const auto& entry : *entry_list) {
    if (!entry.is_dict()) {
      continue;
    }

    auto& entry_dict = entry.GetDict();
    std::optional<int> entry_shill_id =
        entry_dict.FindInt(is_owner ? shill::kP2PGroupInfoShillIDProperty
                                    : shill::kP2PClientInfoShillIDProperty);
    std::optional<int> entry_frequency =
        entry_dict.FindInt(is_owner ? shill::kP2PGroupInfoFrequencyProperty
                                    : shill::kP2PClientInfoFrequencyProperty);
    std::optional<int> entry_network_id =
        entry_dict.FindInt(is_owner ? shill::kP2PGroupInfoNetworkIDProperty
                                    : shill::kP2PClientInfoNetworkIDProperty);
    if (!entry_shill_id) {
      NET_LOG(ERROR) << "Missing shill id in Wifi Direct group";
      continue;
    }
    if (*entry_shill_id != shill_id) {
      NET_LOG(EVENT) << "Found mis-match Wifi Direct group with shill_id: "
                     << *entry_shill_id << ", skipped.";
      continue;
    }
    if (!entry_frequency) {
      NET_LOG(ERROR) << "Missing frequency property in Wifi Direct group";
      std::move(callback).Run(OperationResult::kInvalidGroupProperties,
                              /*metadata=*/std::nullopt);
      return;
    }
    if (!entry_network_id) {
      NET_LOG(ERROR) << "Missing network id property in Wifi Direct group";
      std::move(callback).Run(OperationResult::kInvalidGroupProperties,
                              /*metadata=*/std::nullopt);
      return;
    }

    std::move(callback).Run(
        OperationResult::kSuccess,
        WifiDirectConnectionMetadata{shill_id,
                                     static_cast<uint32_t>(*entry_frequency),
                                     *entry_network_id});
    return;
  }

  NET_LOG(ERROR) << "Did not find the matched P2P group info with shill_id: "
                 << shill_id;
  std::move(callback).Run(OperationResult::kInvalidGroupProperties,
                          /*metadata=*/std::nullopt);
}

void WifiP2PController::OnCreateOrConnectP2PGroupFailure(
    WifiP2PGroupCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Create or connect to P2PGroup failed due to DBus error: "
                 << error_name << ", message: " << error_message;
  std::move(callback).Run(OperationResult::kDBusError,
                          /*metadata=*/std::nullopt);
}

void WifiP2PController::ConnectToWifiP2PGroup(const std::string& ssid,
                                              const std::string& passphrase,
                                              std::optional<uint32_t> frequency,
                                              WifiP2PGroupCallback callback) {
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->ConnectToP2PGroup(
      ShillManagerClient::ConnectP2PGroupParameter{
          ssid,
          passphrase,
          frequency,
          /*priority=*/std::nullopt,
      },
      base::BindOnce(&WifiP2PController::OnCreateOrConnectP2PGroupSuccess,
                     weak_ptr_factory_.GetWeakPtr(), /*create_group=*/false,
                     std::move(callback_split.first)),
      base::BindOnce(&WifiP2PController::OnCreateOrConnectP2PGroupFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.second)));
}

}  // namespace ash
