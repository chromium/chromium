// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

// TODO (jiajunz): Use shill constants after they are added.
const char kShillTetheringBand2_4GHz[] = "2.4GHz";
const char kShillTetheringBand5GHz[] = "5GHz";
const char kShillInvalidProperties[] = "invalid_properties";
const char kShillUpstreamNotReady[] = "upstream_not_ready";
const char kShillNetworkingFailure[] = "network_failure";
const char kShillWifiDriverFailure[] = "wifi_driver_failure";
const char kShillCellularAttachFailure[] = "cellular_attach_failure";
const char kShillNoUpstreamConnection[] = "no_upstream";
const char kShillEnableTetheringSuccess[] = "success";

hotspot_config::mojom::WiFiBand ShillBandToMojom(
    const std::string& shill_band) {
  using hotspot_config::mojom::WiFiBand;

  if (shill_band == kShillTetheringBand2_4GHz) {
    return WiFiBand::k2_4GHz;
  }
  if (shill_band == kShillTetheringBand5GHz) {
    return WiFiBand::k5GHz;
  }
  NOTREACHED() << "Unexpected shill tethering band: " << shill_band;
  return WiFiBand::k5GHz;
}

std::string MojomBandToString(hotspot_config::mojom::WiFiBand mojom_band) {
  using hotspot_config::mojom::WiFiBand;

  switch (mojom_band) {
    case WiFiBand::k2_4GHz:
      return kShillTetheringBand2_4GHz;
    case WiFiBand::k5GHz:
      return kShillTetheringBand5GHz;
  }
}

std::string MojomSecurityToString(
    hotspot_config::mojom::WiFiSecurityMode mojom_security) {
  using hotspot_config::mojom::WiFiSecurityMode;

  switch (mojom_security) {
    case WiFiSecurityMode::kWpa2:
      return shill::kSecurityWpa2;
    case WiFiSecurityMode::kWpa3:
      return shill::kSecurityWpa3;
    case WiFiSecurityMode::kWpa2Wpa3:
      return shill::kSecurityWpa2Wpa3;
  }
}

std::string HexEncode(const std::string& ssid) {
  return base::HexEncode(ssid.c_str(), ssid.size());
}

std::string HexDecode(const std::string& hex_ssid) {
  std::string ssid;
  if (!base::HexStringToString(hex_ssid, &ssid)) {
    NET_LOG(ERROR) << "Error decoding HexSSID: " << hex_ssid;
  }

  return ssid;
}

}  // namespace

hotspot_config::mojom::HotspotState ShillTetheringStateToMojomState(
    const std::string& shill_state) {
  using hotspot_config::mojom::HotspotState;

  if (shill_state == shill::kTetheringStateActive) {
    return HotspotState::kEnabled;
  }

  if (shill_state == shill::kTetheringStateIdle) {
    return HotspotState::kDisabled;
  }

  if (shill_state == shill::kTetheringStateStarting) {
    return HotspotState::kEnabling;
  }

  if (shill_state == shill::kTetheringStateStopping) {
    return HotspotState::kDisabling;
  }

  NOTREACHED() << "Unexpected shill tethering state: " << shill_state;
  return HotspotState::kDisabled;
}

hotspot_config::mojom::WiFiSecurityMode ShillSecurityToMojom(
    const std::string& shill_security) {
  using hotspot_config::mojom::WiFiSecurityMode;

  if (shill_security == shill::kSecurityWpa2) {
    return WiFiSecurityMode::kWpa2;
  }
  if (shill_security == shill::kSecurityWpa3) {
    return WiFiSecurityMode::kWpa3;
  }
  if (shill_security == shill::kSecurityWpa2Wpa3) {
    return WiFiSecurityMode::kWpa2Wpa3;
  }

  NOTREACHED() << "Unexpeted shill tethering security mode: " << shill_security;
  return WiFiSecurityMode::kWpa2;
}

hotspot_config::mojom::HotspotConfigPtr ShillTetheringConfigToMojomConfig(
    const base::Value& shill_tethering_config) {
  using hotspot_config::mojom::HotspotConfig;

  auto result = HotspotConfig::New();
  absl::optional<bool> auto_disable = shill_tethering_config.GetDict().FindBool(
      shill::kTetheringConfAutoDisableProperty);
  if (!auto_disable) {
    NET_LOG(ERROR) << "Auto_disable not found in tethering config.";
  }
  result->auto_disable = auto_disable.value_or(true);
  const std::string* wifi_band = shill_tethering_config.GetDict().FindString(
      shill::kTetheringConfBandProperty);
  if (!wifi_band) {
    NET_LOG(ERROR) << "WiFi band not found in tethering config.";
    result->band = hotspot_config::mojom::WiFiBand::k5GHz;
  } else {
    result->band = ShillBandToMojom(*wifi_band);
  }

  const std::string* security = shill_tethering_config.GetDict().FindString(
      shill::kTetheringConfSecurityProperty);
  if (!security) {
    NET_LOG(ERROR) << "WiFi security mode not found in tethering config.";
    result->security = hotspot_config::mojom::WiFiSecurityMode::kWpa2;
  } else {
    result->security = ShillSecurityToMojom(*security);
  }

  const std::string* ssid = shill_tethering_config.GetDict().FindString(
      shill::kTetheringConfSSIDProperty);
  if (!ssid) {
    NET_LOG(ERROR) << "SSID not found in tethering config.";
  }
  result->ssid = ssid ? HexDecode(*ssid) : std::string();
  const std::string* passphrase = shill_tethering_config.GetDict().FindString(
      shill::kTetheringConfPassphraseProperty);
  if (!passphrase) {
    NET_LOG(ERROR) << "Passphrase not found in tethering config.";
  }
  result->passphrase = passphrase ? *passphrase : std::string();
  absl::optional<bool> bssid_randomization =
      shill_tethering_config.GetDict().FindBool(
          shill::kTetheringConfMARProperty);
  if (!bssid_randomization) {
    NET_LOG(ERROR) << shill::kTetheringConfMARProperty
                   << " not found in tethering config.";
  }
  // Default to true for privacy concern, specifically, to lower the possibility
  // of a user tracking.
  result->bssid_randomization = bssid_randomization.value_or(true);
  return result;
}

base::Value MojomConfigToShillConfig(
    const hotspot_config::mojom::HotspotConfigPtr mojom_config) {
  using hotspot_config::mojom::HotspotConfig;

  base::Value result(base::Value::Type::DICTIONARY);
  result.GetDict().Set(shill::kTetheringConfAutoDisableProperty,
                       base::Value(mojom_config->auto_disable));
  result.GetDict().Set(shill::kTetheringConfBandProperty,
                       base::Value(MojomBandToString(mojom_config->band)));
  result.GetDict().Set(
      shill::kTetheringConfSecurityProperty,
      base::Value(MojomSecurityToString(mojom_config->security)));
  result.GetDict().Set(shill::kTetheringConfSSIDProperty,
                       base::Value(HexEncode(mojom_config->ssid)));
  result.GetDict().Set(shill::kTetheringConfPassphraseProperty,
                       base::Value(mojom_config->passphrase));
  result.GetDict().Set(shill::kTetheringConfMARProperty,
                       base::Value(mojom_config->bssid_randomization));
  return result;
}

hotspot_config::mojom::HotspotControlResult SetTetheringEnabledResultToMojom(
    const std::string& shill_enabled_result) {
  using hotspot_config::mojom::HotspotControlResult;

  if (shill_enabled_result == kShillEnableTetheringSuccess) {
    return HotspotControlResult::kSuccess;
  }
  if (shill_enabled_result == kShillInvalidProperties) {
    return HotspotControlResult::kInvalidConfiguration;
  }
  if (shill_enabled_result == kShillUpstreamNotReady) {
    return HotspotControlResult::kUpstreamNotReady;
  }
  if (shill_enabled_result == kShillNetworkingFailure) {
    return HotspotControlResult::kNetworkSetupFailure;
  }
  if (shill_enabled_result == kShillWifiDriverFailure) {
    return HotspotControlResult::kWifiDriverFailure;
  }
  if (shill_enabled_result == kShillCellularAttachFailure) {
    return HotspotControlResult::kCellularAttachFailure;
  }
  if (shill_enabled_result == kShillNoUpstreamConnection) {
    return HotspotControlResult::kNoUpstreamConnection;
  }

  NET_LOG(ERROR) << "Unknown enable/disable tethering error: "
                 << shill_enabled_result;
  return HotspotControlResult::kUnknownFailure;
}

}  // namespace ash