// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/network/shill_service_util.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

ShillServiceInfo::ShillServiceInfo(unsigned int id, std::string service_type)
    : service_name_(base::StringPrintf("service_name_%u", id)),
      service_path_(base::StringPrintf("service_path_%u", id)),
      service_guid_(base::StringPrintf("service_guid_%u", id)),
      service_type_(std::move(service_type)) {
  const std::array<std::string, 4> valid_service_types = {
      shill::kTypeWifi, shill::kTypeCellular, shill::kTypeEthernet,
      shill::kTypeVPN};
  CHECK(base::Contains(valid_service_types, service_type_));
}

ShillServiceInfo::~ShillServiceInfo() = default;

void ShillServiceInfo::ConfigureService(bool connected) const {
  ShillServiceClient::Get()->GetTestInterface()->AddService(
      service_path_, service_guid_, service_name_, service_type_,
      shill::kStateIdle, /*visible=*/true);
  ShillProfileClient::Get()->GetTestInterface()->AddService(
      ShillProfileClient::GetSharedProfilePath(), service_path_);

  // Marking this service as connectable since networks other than Ethernet
  // are not connectable by default.
  ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
      service_path_, shill::kConnectableProperty, base::Value(true));

  if (connected) {
    ConnectShillService(service_path_);
  }
}

void ConnectShillService(const std::string& service_path) {
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      /*check_error_state=*/false, ConnectCallbackMode::ON_COMPLETED);
}

void DisconnectShillService(const std::string& service_path) {
  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      service_path, base::DoNothing(), base::DoNothing());
}

}  // namespace ash
