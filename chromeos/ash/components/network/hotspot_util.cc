// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_util.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

hotspot_config::mojom::WiFiBand ShillBandToMojom(
    const std::string& shill_band) {
  using hotspot_config::mojom::WiFiBand;

  if (shill_band == shill::kBand2GHz) {
    return WiFiBand::k2_4GHz;
  }
  if (shill_band == shill::kBandAll) {
    return WiFiBand::kAutoChoose;
  }
  NET_LOG(ERROR) << "Unexpected shill tethering band: " << shill_band;
  return WiFiBand::kAutoChoose;
}

std::string MojomBandToString(hotspot_config::mojom::WiFiBand mojom_band) {
  using hotspot_config::mojom::WiFiBand;

  switch (mojom_band) {
    case WiFiBand::k2_4GHz:
      return shill::kBand2GHz;
    case WiFiBand::kAutoChoose:
      return shill::kBandAll;
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

  if (shill_state == shill::kTetheringStateStarting ||
      shill_state == shill::kTetheringStateRestarting) {
    return HotspotState::kEnabling;
  }

  if (shill_state == shill::kTetheringStateStopping) {
    return HotspotState::kDisabling;
  }

  NET_LOG(ERROR) << "Unexpected shill tethering state: " << shill_state;
  return HotspotState::kDisabled;
}

hotspot_config::mojom::DisableReason ShillTetheringIdleReasonToMojomState(
    const std::string& idle_reason) {
  using hotspot_config::mojom::DisableReason;

  if (idle_reason == shill::kTetheringIdleReasonInactive) {
    return DisableReason::kAutoDisabled;
  }

  if (idle_reason == shill::kTetheringIdleReasonUpstreamDisconnect) {
    return DisableReason::kUpstreamNetworkNotAvailable;
  }

  if (idle_reason == shill::kTetheringIdleReasonError) {
    return DisableReason::kInternalError;
  }

  if (idle_reason == shill::kTetheringIdleReasonSuspend) {
    return DisableReason::kSuspended;
  }

  if (idle_reason == shill::kTetheringIdleReasonUserExit ||
      idle_reason == shill::kTetheringIdleReasonClientStop) {
    return DisableReason::kUserInitiated;
  }

  if (idle_reason == shill::kTetheringIdleReasonConfigChange) {
    return DisableReason::kRestart;
  }

  if (idle_reason == shill::kTetheringIdleReasonUpstreamNoInternet) {
    return DisableReason::kUpstreamNoInternet;
  }

  if (idle_reason == shill::kTetheringIdleReasonDownstreamLinkDisconnect) {
    return DisableReason::kDownstreamLinkDisconnect;
  }

  if (idle_reason == shill::kTetheringIdleReasonDownstreamNetworkDisconnect) {
    return DisableReason::kDownstreamNetworkDisconnect;
  }

  if (idle_reason == shill::kTetheringIdleReasonStartTimeout) {
    return DisableReason::kStartTimeout;
  }

  if (idle_reason == shill::kTetheringIdleReasonUpstreamNotAvailable) {
    return DisableReason::kUpstreamNotAvailable;
  }
  if (idle_reason == shill::kTetheringIdleReasonResourceBusy) {
    return DisableReason::kResourceBusy;
  }

  NET_LOG(ERROR) << "Unexpected idle reason: " << idle_reason;
  return DisableReason::kUnknownError;
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

  NET_LOG(ERROR) << "Unexpeted shill tethering security mode: "
                 << shill_security;
  return WiFiSecurityMode::kWpa2;
}

hotspot_config::mojom::HotspotConfigPtr ShillTetheringConfigToMojomConfig(
    const base::Value::Dict& shill_tethering_config) {
  using hotspot_config::mojom::HotspotConfig;

  auto result = HotspotConfig::New();
  std::optional<bool> auto_disable =
      shill_tethering_config.FindBool(shill::kTetheringConfAutoDisableProperty);
  if (!auto_disable) {
    NET_LOG(ERROR) << "Auto_disable not found in tethering config.";
  }
  result->auto_disable = auto_disable.value_or(true);
  const std::string* wifi_band =
      shill_tethering_config.FindString(shill::kTetheringConfBandProperty);
  if (!wifi_band) {
    NET_LOG(ERROR) << "WiFi band not found in tethering config.";
    result->band = hotspot_config::mojom::WiFiBand::kAutoChoose;
  } else {
    result->band = ShillBandToMojom(*wifi_band);
  }

  const std::string* security =
      shill_tethering_config.FindString(shill::kTetheringConfSecurityProperty);
  if (!security) {
    NET_LOG(ERROR) << "WiFi security mode not found in tethering config.";
    result->security = hotspot_config::mojom::WiFiSecurityMode::kWpa2;
  } else {
    result->security = ShillSecurityToMojom(*security);
  }

  const std::string* ssid =
      shill_tethering_config.FindString(shill::kTetheringConfSSIDProperty);
  if (!ssid) {
    NET_LOG(ERROR) << "SSID not found in tethering config.";
  }
  result->ssid = ssid ? HexDecode(*ssid) : std::string();
  const std::string* passphrase = shill_tethering_config.FindString(
      shill::kTetheringConfPassphraseProperty);
  if (!passphrase) {
    NET_LOG(ERROR) << "Passphrase not found in tethering config.";
  }
  result->passphrase = passphrase ? *passphrase : std::string();
  std::optional<bool> bssid_randomization =
      shill_tethering_config.FindBool(shill::kTetheringConfMARProperty);
  if (!bssid_randomization) {
    NET_LOG(ERROR) << shill::kTetheringConfMARProperty
                   << " not found in tethering config.";
  }
  // Default to true for privacy concern, specifically, to lower the possibility
  // of a user tracking.
  result->bssid_randomization = bssid_randomization.value_or(true);
  return result;
}

base::Value::Dict MojomConfigToShillConfig(
    const hotspot_config::mojom::HotspotConfigPtr mojom_config) {
  using hotspot_config::mojom::HotspotConfig;

  base::Value::Dict result;
  result.Set(shill::kTetheringConfAutoDisableProperty,
             mojom_config->auto_disable);
  result.Set(shill::kTetheringConfBandProperty,
             MojomBandToString(mojom_config->band));
  result.Set(shill::kTetheringConfSecurityProperty,
             MojomSecurityToString(mojom_config->security));
  result.Set(shill::kTetheringConfSSIDProperty,
             base::HexEncode(mojom_config->ssid));
  result.Set(shill::kTetheringConfPassphraseProperty, mojom_config->passphrase);
  result.Set(shill::kTetheringConfMARProperty,
             mojom_config->bssid_randomization);
  return result;
}

hotspot_config::mojom::HotspotControlResult SetTetheringEnabledResultToMojom(
    const std::string& shill_enabled_result) {
  using hotspot_config::mojom::HotspotControlResult;

  if (shill_enabled_result == shill::kTetheringEnableResultSuccess) {
    return HotspotControlResult::kSuccess;
  }
  if (shill_enabled_result == shill::kTetheringEnableResultInvalidProperties) {
    return HotspotControlResult::kInvalidConfiguration;
  }
  if (shill_enabled_result ==
      shill::kTetheringEnableResultUpstreamNotAvailable) {
    return HotspotControlResult::kUpstreamNotAvailable;
  }
  if (shill_enabled_result ==
      shill::kTetheringEnableResultNetworkSetupFailure) {
    return HotspotControlResult::kNetworkSetupFailure;
  }
  if (shill_enabled_result ==
      shill::kTetheringEnableResultDownstreamWiFiFailure) {
    return HotspotControlResult::kDownstreamWifiFailure;
  }
  if (shill_enabled_result == shill::kTetheringEnableResultUpstreamFailure) {
    return HotspotControlResult::kUpstreamFailure;
  }
  if (shill_enabled_result == shill::kTetheringEnableResultWrongState) {
    return HotspotControlResult::kAlreadyFulfilled;
  }
  if (shill_enabled_result == shill::kTetheringEnableResultAbort) {
    return HotspotControlResult::kAborted;
  }
  if (shill_enabled_result == shill::kTetheringEnableResultNotAllowed) {
    return HotspotControlResult::kNotAllowed;
  }
  if (shill_enabled_result == shill::kTetheringEnableResultBusy) {
    return HotspotControlResult::kBusy;
  }
  if (shill_enabled_result ==
      shill::kTetheringEnableResultConcurrencyNotSupported) {
    return HotspotControlResult::kConcurrencyNotSupported;
  }
  if (shill_enabled_result == shill::kTetheringEnableResultFailure) {
    return HotspotControlResult::kOperationFailure;
  }

  NET_LOG(ERROR) << "Unknown enable/disable tethering error: "
                 << shill_enabled_result;
  return HotspotControlResult::kUnknownFailure;
}

}  // namespace ash
