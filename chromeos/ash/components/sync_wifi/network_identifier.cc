// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/network_identifier.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/sync_wifi/network_type_conversions.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::sync_wifi {

namespace {

const char kDelimeter[] = "<||>";

}  // namespace

// static
NetworkIdentifier NetworkIdentifier::FromProto(
    const sync_pb::WifiConfigurationSpecifics& specifics) {
  return NetworkIdentifier(
      specifics.hex_ssid(),
      SecurityTypeStringFromProto(specifics.security_type()));
}

// static
NetworkIdentifier NetworkIdentifier::FromMojoNetwork(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr& network) {
  return NetworkIdentifier(
      network->type_state->get_wifi()->hex_ssid,
      SecurityTypeStringFromMojo(network->type_state->get_wifi()->security));
}

// static
NetworkIdentifier NetworkIdentifier::DeserializeFromString(
    const std::string& serialized_string) {
  std::vector<std::string> pieces =
      base::SplitString(serialized_string, kDelimeter, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  DCHECK(pieces.size() == 2);
  return NetworkIdentifier(pieces[0], pieces[1]);
}

// static
NetworkIdentifier NetworkIdentifier::FromNetworkState(
    const NetworkState* network) {
  return NetworkIdentifier(network->GetHexSsid(), network->security_class());
}

NetworkIdentifier::NetworkIdentifier(const std::string& hex_ssid,
                                     const std::string& security_type)
    : security_type_(security_type) {
  hex_ssid_ =
      base::StartsWith(hex_ssid, "0x", base::CompareCase::INSENSITIVE_ASCII)
          ? base::ToUpperASCII(hex_ssid.substr(2))
          : base::ToUpperASCII(hex_ssid);
}

NetworkIdentifier::~NetworkIdentifier() = default;

std::string NetworkIdentifier::SerializeToString() const {
  return base::StringPrintf("%s%s%s", hex_ssid_.c_str(), kDelimeter,
                            security_type_.c_str());
}

bool NetworkIdentifier::IsValid() const {
  return !hex_ssid_.empty() && !security_type_.empty();
}

bool NetworkIdentifier::operator==(const NetworkIdentifier& o) const {
  // Invalid networks are not equal to each other.
  if (!IsValid() || !o.IsValid()) {
    return false;
  }

  return hex_ssid_ == o.hex_ssid_ && security_type_ == o.security_type_;
}

bool NetworkIdentifier::operator!=(const NetworkIdentifier& o) const {
  // Invalid networks are not equal to each other.
  if (!IsValid() || !o.IsValid()) {
    return true;
  }

  return hex_ssid_ != o.hex_ssid_ || security_type_ != o.security_type_;
}

bool NetworkIdentifier::operator>(const NetworkIdentifier& o) const {
  return *this != o && !(*this < o);
}

bool NetworkIdentifier::operator<(const NetworkIdentifier& o) const {
  return SerializeToString() < o.SerializeToString();
}

}  // namespace ash::sync_wifi
