// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_group.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_metrics_logger.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

WifiP2PController* g_wifi_p2p_controller = nullptr;

WifiP2PController::OperationResult ShillResultToEnum(
    const std::string& shill_result_code) {
  if (shill_result_code == shill::kCreateP2PGroupResultSuccess ||
      shill_result_code == shill::kConnectToP2PGroupResultSuccess ||
      shill_result_code == shill::kDestroyP2PGroupResultSuccess ||
      shill_result_code == shill::kDisconnectFromP2PGroupResultSuccess) {
    return WifiP2PController::OperationResult::kSuccess;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultNotAllowed ||
      shill_result_code == shill::kConnectToP2PGroupResultNotAllowed ||
      shill_result_code == shill::kDestroyP2PGroupResultNotAllowed ||
      shill_result_code == shill::kDisconnectFromP2PGroupResultNotAllowed) {
    return WifiP2PController::OperationResult::kNotAllowed;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultNotSupported ||
      shill_result_code == shill::kConnectToP2PGroupResultNotSupported ||
      shill_result_code == shill::kDestroyP2PGroupResultNotSupported ||
      shill_result_code == shill::kDisconnectFromP2PGroupResultNotSupported) {
    return WifiP2PController::OperationResult::kNotSupported;
  }
  if (shill_result_code ==
          shill::kCreateP2PGroupResultConcurrencyNotSupported ||
      shill_result_code ==
          shill::kConnectToP2PGroupResultConcurrencyNotSupported) {
    return WifiP2PController::OperationResult::kConcurrencyNotSupported;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultTimeout ||
      shill_result_code == shill::kConnectToP2PGroupResultTimeout ||
      shill_result_code == shill::kDestroyP2PGroupResultTimeout ||
      shill_result_code == shill::kDisconnectFromP2PGroupResultTimeout) {
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
      shill_result_code == shill::kConnectToP2PGroupResultOperationInProgress ||
      shill_result_code == shill::kDestroyP2PGroupResultOperationInProgress ||
      shill_result_code ==
          shill::kDisconnectFromP2PGroupResultOperationInProgress) {
    return WifiP2PController::OperationResult::kOperationInProgress;
  }
  if (shill_result_code == shill::kCreateP2PGroupResultOperationFailed ||
      shill_result_code == shill::kConnectToP2PGroupResultOperationFailed ||
      shill_result_code == shill::kDestroyP2PGroupResultOperationFailed ||
      shill_result_code ==
          shill::kDisconnectFromP2PGroupResultOperationFailed) {
    return WifiP2PController::OperationResult::kOperationFailed;
  }
  if (shill_result_code == shill::kConnectToP2PGroupResultAuthFailure) {
    return WifiP2PController::OperationResult::kAuthFailure;
  }
  if (shill_result_code == shill::kConnectToP2PGroupResultGroupNotFound ||
      shill_result_code == shill::kDestroyP2PGroupResultNoGroup) {
    return WifiP2PController::OperationResult::kGroupNotFound;
  }
  if (shill_result_code == shill::kConnectToP2PGroupResultAlreadyConnected) {
    return WifiP2PController::OperationResult::kAlreadyConnected;
  }
  if (shill_result_code == shill::kDisconnectFromP2PGroupResultNotConnected) {
    return WifiP2PController::OperationResult::kNotConnected;
  }

  NET_LOG(ERROR) << "Unexpected result code: " << shill_result_code;
  return WifiP2PController::OperationResult::kInvalidResultCode;
}

WifiP2PController::OperationResult GetOperationResult(
    const base::Value::Dict& result_dict) {
  const std::string* result_code =
      result_dict.FindString(shill::kP2PResultCode);
  if (!result_code) {
    return WifiP2PController::OperationResult::kInvalidResultCode;
  }

  return ShillResultToEnum(*result_code);
}

bool IsDigitOrAlpha(char c) {
  return std::isdigit(c) || std::isalpha(c);
}

// WiFi Direct ssid should follows WiFi Direct v1.9 3.2.1 (I.e. must begin with
// DIRECT-xy where x and y are random letters/numbers). The passphrase must be
// at least 8 character long.
bool ValidateWifiDirectCredentails(const std::string& ssid,
                                   const std::string& passphrase) {
  if (ssid.length() != 9 || !ssid.starts_with("DIRECT-") ||
      !IsDigitOrAlpha(ssid[7]) || !IsDigitOrAlpha(ssid[8])) {
    NET_LOG(ERROR) << "Invalid SSID for WiFi Direct.";
    return false;
  }
  if (passphrase.length() < 8) {
    NET_LOG(ERROR) << "Invalid passphrase for WiFi Direct.";
    return false;
  }
  return true;
}

}  // namespace

WifiP2PController::WifiP2PController() = default;

WifiP2PController::~WifiP2PController() {
  if (ShillManagerClient::Get()) {
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  }
}

void WifiP2PController::Init() {
  ShillManagerClient::Get()->SetProperty(
      shill::kP2PAllowedProperty,
      base::Value(ash::features::IsWifiDirectEnabled()), base::DoNothing(),
      base::BindOnce(&WifiP2PController::OnSetManagerPropertyFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     shill::kP2PAllowedProperty));

  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&WifiP2PController::OnGetManagerProperties,
                     weak_ptr_factory_.GetWeakPtr()));
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

void WifiP2PController::CreateWifiP2PGroup(
    std::optional<std::string> ssid,
    std::optional<std::string> passphrase,
    WifiP2PGroupCallback callback) {
  CHECK(ssid.has_value() == passphrase.has_value());
  if (ssid && passphrase &&
      !ValidateWifiDirectCredentails(*ssid, *passphrase)) {
    CompleteWifiP2PGroupCallback(
        OperationType::kCreateGroup, OperationResult::kInvalidArguments,
        std::move(callback), /*metadata=*/std::nullopt);
    return;
  }

  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->CreateP2PGroup(
      ShillManagerClient::CreateP2PGroupParameter{ssid, passphrase},
      base::BindOnce(&WifiP2PController::OnCreateOrConnectP2PGroupSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kCreateGroup,
                     std::move(callback_split.first)),
      base::BindOnce(&WifiP2PController::OnCreateOrConnectP2PGroupFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kCreateGroup,
                     std::move(callback_split.second)));
}

void WifiP2PController::OnCreateOrConnectP2PGroupSuccess(
    const OperationType& type,
    WifiP2PGroupCallback callback,
    base::Value::Dict result_dict) {
  CHECK(type == OperationType::kCreateGroup ||
        type == OperationType::kConnectGroup);
  NET_LOG(EVENT) << type << " operation completed with result: " << result_dict;

  const OperationResult result = GetOperationResult(result_dict);
  if (result != OperationResult::kSuccess) {
    CompleteWifiP2PGroupCallback(type, result, std::move(callback),
                                 /*metadata=*/std::nullopt);
    return;
  }

  std::optional<int> shill_id = result_dict.FindInt(shill::kP2PDeviceShillID);
  if (!shill_id) {
    NET_LOG(ERROR) << "Missing shill_id in Wifi direct operation response when "
                      "result code is success";
    CompleteWifiP2PGroupCallback(type, OperationResult::kInvalidResultCode,
                                 std::move(callback),
                                 /*metadata=*/std::nullopt);
    return;
  }

  ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &WifiP2PController::GetP2PGroupMetadata, weak_ptr_factory_.GetWeakPtr(),
      *shill_id, type, std::move(callback)));
}

void WifiP2PController::GetP2PGroupMetadata(
    int shill_id,
    const OperationType& type,
    WifiP2PGroupCallback callback,
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "Error getting Shill manager properties.";
    CompleteWifiP2PGroupCallback(type, OperationResult::kInvalidGroupProperties,
                                 std::move(callback),
                                 /*metadata=*/std::nullopt);
    return;
  }
  const bool is_owner = type == OperationType::kCreateGroup;
  base::Value::List* entry_list =
      properties->FindList(is_owner ? shill::kP2PGroupInfosProperty
                                    : shill::kP2PClientInfosProperty);
  if (!entry_list || entry_list->size() == 0) {
    CompleteWifiP2PGroupCallback(type, OperationResult::kInvalidGroupProperties,
                                 std::move(callback),
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
    const std::string* entry_ipv4_address = entry_dict.FindString(
        is_owner ? shill::kP2PGroupInfoIPv4AddressProperty
                 : shill::kP2PClientInfoIPv4AddressProperty);
    const std::string* entry_ssid =
        entry_dict.FindString(is_owner ? shill::kP2PGroupInfoSSIDProperty
                                       : shill::kP2PClientInfoSSIDProperty);
    const std::string* entry_passphrase = entry_dict.FindString(
        is_owner ? shill::kP2PGroupInfoPassphraseProperty
                 : shill::kP2PClientInfoPassphraseProperty);

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
      CompleteWifiP2PGroupCallback(
          type, OperationResult::kInvalidGroupProperties, std::move(callback),
          /*metadata=*/std::nullopt);
      return;
    }
    if (!entry_network_id) {
      NET_LOG(ERROR) << "Missing network id property in Wifi Direct group";
      CompleteWifiP2PGroupCallback(
          type, OperationResult::kInvalidGroupProperties, std::move(callback),
          /*metadata=*/std::nullopt);
      return;
    }
    if (!entry_ssid) {
      NET_LOG(ERROR) << "Missing ssid property in Wifi Direct group";
      CompleteWifiP2PGroupCallback(
          type, OperationResult::kInvalidGroupProperties, std::move(callback),
          /*metadata=*/std::nullopt);
      return;
    }
    if (!entry_passphrase) {
      NET_LOG(ERROR) << "Missing network id property in Wifi Direct group";
      CompleteWifiP2PGroupCallback(
          type, OperationResult::kInvalidGroupProperties, std::move(callback),
          /*metadata=*/std::nullopt);
      return;
    }
    if (!entry_ipv4_address) {
      NET_LOG(ERROR) << "Missing ipv4 address property in Wifi Direct group";
    }

    CompleteWifiP2PGroupCallback(
        type, OperationResult::kSuccess, std::move(callback),
        WifiP2PGroup{shill_id, static_cast<uint32_t>(*entry_frequency),
                     *entry_network_id,
                     entry_ipv4_address ? *entry_ipv4_address : std::string(),
                     *entry_ssid, *entry_passphrase, is_owner});
    return;
  }

  NET_LOG(ERROR) << "Did not find the matched P2P group info with shill_id: "
                 << shill_id;
  CompleteWifiP2PGroupCallback(type, OperationResult::kInvalidGroupProperties,
                               std::move(callback),
                               /*metadata=*/std::nullopt);
}

void WifiP2PController::OnCreateOrConnectP2PGroupFailure(
    const OperationType& type,
    WifiP2PGroupCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  CHECK(type == OperationType::kCreateGroup ||
        type == OperationType::kConnectGroup);
  NET_LOG(ERROR) << type
                 << " operation failed due to DBus error: " << error_name
                 << ", message: " << error_message;

  CompleteWifiP2PGroupCallback(type, OperationResult::kDBusError,
                               std::move(callback),
                               /*metadata=*/std::nullopt);
}

void WifiP2PController::CompleteWifiP2PGroupCallback(
    const OperationType& type,
    const OperationResult& result,
    WifiP2PGroupCallback callback,
    std::optional<WifiP2PGroup> group_metadata) {
  CHECK(type == OperationType::kCreateGroup ||
        type == OperationType::kConnectGroup);
  WifiP2PMetricsLogger::RecordWifiP2POperationResult(type, result);
  std::move(callback).Run(result, group_metadata);
}

void WifiP2PController::DestroyWifiP2PGroup(
    int shill_id,
    base::OnceCallback<void(OperationResult result)> callback) {
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->DestroyP2PGroup(
      shill_id,
      base::BindOnce(&WifiP2PController::OnDestroyOrDisconnectP2PGroupSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kDestroyGroup,
                     std::move(callback_split.first)),
      base::BindOnce(&WifiP2PController::OnDestroyOrDisconnectP2PGroupFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kDestroyGroup,
                     std::move(callback_split.second)));
}

void WifiP2PController::OnDestroyOrDisconnectP2PGroupSuccess(
    const OperationType& type,
    base::OnceCallback<void(OperationResult result)> callback,
    base::Value::Dict result_dict) {
  CHECK(type == OperationType::kDestroyGroup ||
        type == OperationType::kDisconnectGroup);
  NET_LOG(EVENT) << type << " operation completed with result: " << result_dict;

  const OperationResult result = GetOperationResult(result_dict);
  WifiP2PMetricsLogger::RecordWifiP2POperationResult(type, result);
  std::move(callback).Run(result);
}

void WifiP2PController::OnDestroyOrDisconnectP2PGroupFailure(
    const OperationType& type,
    base::OnceCallback<void(OperationResult result)> callback,
    const std::string& error_name,
    const std::string& error_message) {
  CHECK(type == OperationType::kDestroyGroup ||
        type == OperationType::kDisconnectGroup);
  NET_LOG(ERROR) << type
                 << " operation failed due to  DBus error: " << error_name
                 << ", message: " << error_message;

  WifiP2PMetricsLogger::RecordWifiP2POperationResult(
      type, OperationResult::kDBusError);
  std::move(callback).Run(OperationResult::kDBusError);
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
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kConnectGroup,
                     std::move(callback_split.first)),
      base::BindOnce(&WifiP2PController::OnCreateOrConnectP2PGroupFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kConnectGroup,
                     std::move(callback_split.second)));
}

void WifiP2PController::DisconnectFromWifiP2PGroup(
    int shill_id,
    base::OnceCallback<void(OperationResult result)> callback) {
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->DisconnectFromP2PGroup(
      shill_id,
      base::BindOnce(&WifiP2PController::OnDestroyOrDisconnectP2PGroupSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kDisconnectGroup,
                     std::move(callback_split.first)),
      base::BindOnce(&WifiP2PController::OnDestroyOrDisconnectP2PGroupFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     OperationType::kDisconnectGroup,
                     std::move(callback_split.second)));
}

const WifiP2PController::WifiP2PCapabilities&
WifiP2PController::GetP2PCapabilities() const {
  WifiP2PMetricsLogger::RecordWifiP2PCapabilities(wifi_p2p_capabilities_);
  return wifi_p2p_capabilities_;
}

void WifiP2PController::TagSocket(
    int network_id,
    base::ScopedFD socket_fd,
    base::OnceCallback<void(bool success)> callback) {
  PatchPanelClient::Get()->TagSocket(
      socket_fd.get(), network_id,
      PatchPanelClient::VpnRoutingPolicy::kBypassVpn,
      base::BindOnce(&WifiP2PController::OnTagSocketCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WifiP2PController::OnTagSocketCompleted(
    base::OnceCallback<void(bool success)> callback,
    bool success) {
  if (!success) {
    NET_LOG(ERROR) << "Tag socket operation failed.";
  }
  WifiP2PMetricsLogger::RecordTagSocketOperationResult(success);
  std::move(callback).Run(success);
}

void WifiP2PController::OnPropertyChanged(const std::string& key,
                                          const base::Value& value) {
  if (key == shill::kP2PCapabilitiesProperty) {
    NET_LOG(EVENT) << "WifiP2PController: Registered a property change event "
                      "on kP2PCapabilitiesProperty";
    UpdateP2PCapabilities(value.GetDict());
  }

  if (key == shill::kP2PGroupInfosProperty) {
    CheckAndNotifyDisconnection(
        /*is_owner=*/true, value, shill::kP2PGroupInfoStateProperty,
        shill::kP2PGroupInfoShillIDProperty, shill::kP2PGroupInfoStateIdle);
  }

  if (key == shill::kP2PClientInfosProperty) {
    CheckAndNotifyDisconnection(
        /*is_owner=*/false, value, shill::kP2PClientInfoStateProperty,
        shill::kP2PClientInfoShillIDProperty, shill::kP2PClientInfoStateIdle);
  }
}

void WifiP2PController::CheckAndNotifyDisconnection(
    bool is_owner,
    const base::Value& property_list,
    const std::string& interface_state_property,
    const std::string& shill_id_property,
    const std::string& idle_state_property) {
  for (const base::Value& group_info : property_list.GetList()) {
    const base::Value::Dict& properties = group_info.GetDict();
    const std::string* interface_state =
        properties.FindString(interface_state_property);
    std::optional<int> shill_id = properties.FindInt(shill_id_property);
    if (shill_id.has_value() && interface_state &&
        (*interface_state == idle_state_property)) {
      for (auto& observer : observer_list_) {
        observer.OnWifiDirectConnectionDisconnected(shill_id.value(), is_owner);
      }
    }
  }
}

void WifiP2PController::OnGetManagerProperties(
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR)
        << "WifiP2PController: Failed to get shill manager properties.";
    return;
  }
  const base::Value::Dict* value =
      properties->FindDict(shill::kP2PCapabilitiesProperty);
  if (!value) {
    NET_LOG(ERROR) << "WifiP2PController: No dictionary value for: "
                   << shill::kP2PCapabilitiesProperty;
    return;
  }

  UpdateP2PCapabilities(*value);
}

void WifiP2PController::UpdateP2PCapabilities(
    const base::Value::Dict& capabilities) {
  const std::string* group_readiness =
      capabilities.FindString(shill::kP2PCapabilitiesGroupReadinessProperty);
  const std::string* client_readiness =
      capabilities.FindString(shill::kP2PCapabilitiesClientReadinessProperty);
  const std::optional<bool> p2p_supported =
      capabilities.FindBool(shill::kP2PCapabilitiesP2PSupportedProperty);

  if (group_readiness) {
    wifi_p2p_capabilities_.is_owner_ready =
        (*group_readiness == shill::kP2PCapabilitiesGroupReadinessReady);
  }

  if (client_readiness) {
    wifi_p2p_capabilities_.is_client_ready =
        (*client_readiness == shill::kP2PCapabilitiesClientReadinessReady);
  }

  if (p2p_supported.has_value()) {
    wifi_p2p_capabilities_.is_p2p_supported = p2p_supported.value();
  }
}

void WifiP2PController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void WifiP2PController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::ostream& operator<<(std::ostream& stream,
                         const WifiP2PController::OperationType& type) {
  switch (type) {
    case WifiP2PController::OperationType::kCreateGroup:
      stream << "Create P2P group";
      break;
    case WifiP2PController::OperationType::kConnectGroup:
      stream << "Connect to P2P group";
      break;
    case WifiP2PController::OperationType::kDestroyGroup:
      stream << "Destroy P2P group";
      break;
    case WifiP2PController::OperationType::kDisconnectGroup:
      stream << "Disconnect from P2P group";
      break;
  }
  return stream;
}

}  // namespace ash
