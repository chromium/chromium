// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/fake_network_device_handler.h"

namespace chromeos {

FakeNetworkDeviceHandler::FakeNetworkDeviceHandler() = default;

FakeNetworkDeviceHandler::~FakeNetworkDeviceHandler() = default;

void FakeNetworkDeviceHandler::GetDeviceProperties(
    const std::string& device_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {}

void FakeNetworkDeviceHandler::SetDeviceProperty(
    const std::string& device_path,
    const std::string& property_name,
    const base::Value& value,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::RegisterCellularNetwork(
    const std::string& device_path,
    const std::string& network_id,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::RequirePin(
    const std::string& device_path,
    bool require_pin,
    const std::string& pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::EnterPin(
    const std::string& device_path,
    const std::string& pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::UnblockPin(
    const std::string& device_path,
    const std::string& puk,
    const std::string& new_pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::ChangePin(
    const std::string& device_path,
    const std::string& old_pin,
    const std::string& new_pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::SetCellularAllowRoaming(bool allow_roaming) {}

void FakeNetworkDeviceHandler::SetUsbEthernetMacAddressSource(
    const std::string& source) {}

void FakeNetworkDeviceHandler::SetWifiTDLSEnabled(
    const std::string& ip_or_mac_address,
    bool enabled,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::GetWifiTDLSStatus(
    const std::string& ip_or_mac_address,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::AddWifiWakeOnPacketConnection(
    const net::IPEndPoint& ip_endpoint,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::AddWifiWakeOnPacketOfTypes(
    const std::vector<std::string>& types,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::RemoveWifiWakeOnPacketConnection(
    const net::IPEndPoint& ip_endpoint,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::RemoveWifiWakeOnPacketOfTypes(
    const std::vector<std::string>& types,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

void FakeNetworkDeviceHandler::RemoveAllWifiWakeOnPacketConnections(
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {}

}  // namespace chromeos
