// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/network_type_conversions.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_config {
namespace mojom = ::chromeos::network_config::mojom;
}

namespace ash::sync_wifi {

namespace {

bool IsAutoconnectEnabled(
    sync_pb::WifiConfigurationSpecifics::AutomaticallyConnectOption
        auto_connect) {
  switch (auto_connect) {
    case sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_DISABLED:
      return false;

    case sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_ENABLED:
    case sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_UNSPECIFIED:
      return true;
  }
}

}  // namespace

// Returns an empty string when |base_16| is unable to be decoded from a
// hex string to bytes. This may signify an improperly encoded SSID.
std::string DecodeHexString(const std::string& base_16) {
  std::string decoded;
  DCHECK_EQ(base_16.size() % 2, 0u) << "Must be a multiple of 2";
  decoded.reserve(base_16.size() / 2);

  std::vector<uint8_t> v;
  if (!base::HexStringToBytes(base_16, &v)) {
    NET_LOG(EVENT) << "Failed to decode hex encoded SSID.";
    return std::string();
  }

  decoded.assign(reinterpret_cast<const char*>(v.data()), v.size());
  return decoded;
}

std::string SecurityTypeStringFromMojo(
    const network_config::mojom::SecurityType& security_type) {
  switch (security_type) {
    case network_config::mojom::SecurityType::kWpaPsk:
      return shill::kSecurityClassPsk;
    case network_config::mojom::SecurityType::kWepPsk:
      return shill::kSecurityClassWep;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      return "";
  }
}

std::string SecurityTypeStringFromProto(
    const sync_pb::WifiConfigurationSpecifics_SecurityType& security_type) {
  switch (security_type) {
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_PSK:
      return shill::kSecurityClassPsk;
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_WEP:
      return shill::kSecurityClassWep;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

sync_pb::WifiConfigurationSpecifics_SecurityType SecurityTypeProtoFromMojo(
    const network_config::mojom::SecurityType& security_type) {
  switch (security_type) {
    case network_config::mojom::SecurityType::kWpaPsk:
      return sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_PSK;
    case network_config::mojom::SecurityType::kWepPsk:
      return sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_WEP;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED_IN_MIGRATION();
      return sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_NONE;
  }
}

sync_pb::WifiConfigurationSpecifics_AutomaticallyConnectOption
AutomaticallyConnectProtoFromMojo(
    const network_config::mojom::ManagedBooleanPtr& auto_connect) {
  if (!auto_connect) {
    return sync_pb::WifiConfigurationSpecifics::
        AUTOMATICALLY_CONNECT_UNSPECIFIED;
  }

  if (auto_connect->active_value) {
    return sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_ENABLED;
  }

  return sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_DISABLED;
}

sync_pb::WifiConfigurationSpecifics_IsPreferredOption IsPreferredProtoFromMojo(
    const network_config::mojom::ManagedInt32Ptr& is_preferred) {
  if (!is_preferred) {
    return sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_UNSPECIFIED;
  }

  if (is_preferred->active_value == 1) {
    return sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_ENABLED;
  }

  return sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_DISABLED;
}

sync_pb::WifiConfigurationSpecifics_ProxyConfiguration_ProxyOption
ProxyOptionProtoFromMojo(
    const network_config::mojom::ManagedProxySettingsPtr& proxy_settings,
    bool is_unspecified) {
  if (!proxy_settings || is_unspecified) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_UNSPECIFIED;
  }

  if (proxy_settings->type->active_value == ::onc::proxy::kPAC) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_AUTOMATIC;
  }

  if (proxy_settings->type->active_value == ::onc::proxy::kWPAD) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_AUTODISCOVERY;
  }

  if (proxy_settings->type->active_value == ::onc::proxy::kManual) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_MANUAL;
  }

  return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
      PROXY_OPTION_DISABLED;
}

sync_pb::WifiConfigurationSpecifics_ProxyConfiguration
ProxyConfigurationProtoFromMojo(
    const network_config::mojom::ManagedProxySettingsPtr& proxy_settings,
    bool is_unspecified) {
  sync_pb::WifiConfigurationSpecifics_ProxyConfiguration proto;
  proto.set_proxy_option(
      ProxyOptionProtoFromMojo(proxy_settings, is_unspecified));

  if (proto.proxy_option() ==
      sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
          PROXY_OPTION_AUTOMATIC) {
    if (proxy_settings->pac) {
      proto.set_autoconfiguration_url(proxy_settings->pac->active_value);
    }
  } else if (proto.proxy_option() ==
             sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
                 PROXY_OPTION_MANUAL) {
    sync_pb::
        WifiConfigurationSpecifics_ProxyConfiguration_ManualProxyConfiguration*
            manual_settings = proto.mutable_manual_proxy_configuration();

    if (proxy_settings->manual->http_proxy) {
      manual_settings->set_http_proxy_url(
          proxy_settings->manual->http_proxy->host->active_value);
      manual_settings->set_http_proxy_port(
          proxy_settings->manual->http_proxy->port->active_value);
    }

    if (proxy_settings->manual->secure_http_proxy) {
      manual_settings->set_secure_http_proxy_url(
          proxy_settings->manual->secure_http_proxy->host->active_value);
      manual_settings->set_secure_http_proxy_port(
          proxy_settings->manual->secure_http_proxy->port->active_value);
    }

    if (proxy_settings->manual->socks) {
      manual_settings->set_socks_host_url(
          proxy_settings->manual->socks->host->active_value);
      manual_settings->set_socks_host_port(
          proxy_settings->manual->socks->port->active_value);
    }

    if (proxy_settings->exclude_domains) {
      for (const std::string& domain :
           proxy_settings->exclude_domains->active_value) {
        manual_settings->add_excluded_domains(domain);
      }
    }
  }

  return proto;
}

network_config::mojom::SecurityType MojoSecurityTypeFromProto(
    const sync_pb::WifiConfigurationSpecifics_SecurityType& security_type) {
  switch (security_type) {
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_PSK:
      return network_config::mojom::SecurityType::kWpaPsk;
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_WEP:
      return network_config::mojom::SecurityType::kWepPsk;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED_IN_MIGRATION();
      return network_config::mojom::SecurityType::kNone;
  }
}

network_config::mojom::ProxySettingsPtr MojoProxySettingsFromProto(
    const sync_pb::WifiConfigurationSpecifics_ProxyConfiguration& proxy_proto) {
  auto proxy_settings = network_config::mojom::ProxySettings::New();
  switch (proxy_proto.proxy_option()) {
    case sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_AUTOMATIC:
      proxy_settings->type = ::onc::proxy::kPAC;
      proxy_settings->pac = proxy_proto.autoconfiguration_url();
      break;
    case sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_AUTODISCOVERY:
      proxy_settings->type = ::onc::proxy::kWPAD;
      break;
    case sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_MANUAL: {
      auto manual_settings = network_config::mojom::ManualProxySettings::New();
      auto synced_manual_configuration =
          proxy_proto.manual_proxy_configuration();
      proxy_settings->type = ::onc::proxy::kManual;

      manual_settings->http_proxy = network_config::mojom::ProxyLocation::New();
      manual_settings->http_proxy->host =
          synced_manual_configuration.http_proxy_url();
      manual_settings->http_proxy->port =
          synced_manual_configuration.http_proxy_port();

      manual_settings->secure_http_proxy =
          network_config::mojom::ProxyLocation::New();
      manual_settings->secure_http_proxy->host =
          synced_manual_configuration.secure_http_proxy_url();
      manual_settings->secure_http_proxy->port =
          synced_manual_configuration.secure_http_proxy_port();

      manual_settings->socks = network_config::mojom::ProxyLocation::New();
      manual_settings->socks->host =
          synced_manual_configuration.socks_host_url();
      manual_settings->socks->port =
          synced_manual_configuration.socks_host_port();

      proxy_settings->manual = std::move(manual_settings);

      std::vector<std::string> exclude_domains;
      for (const std::string& domain :
           synced_manual_configuration.excluded_domains()) {
        exclude_domains.push_back(domain);
      }
      proxy_settings->exclude_domains = std::move(exclude_domains);
      break;
    }
    case sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_DISABLED:
      proxy_settings->type = ::onc::proxy::kDirect;
      break;
    case sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_UNSPECIFIED:
      break;
  }

  return proxy_settings;
}

network_config::mojom::ConfigPropertiesPtr MojoNetworkConfigFromProto(
    const sync_pb::WifiConfigurationSpecifics& specifics) {
  auto config = network_config::mojom::ConfigProperties::New();
  auto wifi = network_config::mojom::WiFiConfigProperties::New();

  wifi->ssid = DecodeHexString(specifics.hex_ssid());
  if (wifi->ssid->empty()) {
    // Return early instead of populating the other fields for a WifiConfig
    // because the SSID is the primary key for the network, so without this,
    // the rest of the properties are irrelevant.
    return nullptr;
  }

  wifi->security = MojoSecurityTypeFromProto(specifics.security_type());
  wifi->passphrase = specifics.passphrase();
  wifi->hidden_ssid = network_config::mojom::HiddenSsidMode::kDisabled;

  config->type_config =
      network_config::mojom::NetworkTypeConfigProperties::NewWifi(
          std::move(wifi));

  config->auto_connect = network_config::mojom::AutoConnectConfig::New(
      IsAutoconnectEnabled(specifics.automatically_connect()));

  if (specifics.has_is_preferred() &&
      specifics.is_preferred() !=
          sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_UNSPECIFIED) {
    config->priority = network_config::mojom::PriorityConfig::New(
        specifics.is_preferred() ==
                sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_ENABLED
            ? 1
            : 0);
  }

  // TODO(crbug/1128692): Restore support for the metered property when mojo
  // networks track the "Automatic" state.

  // For backwards compatibility, any available custom nameservers are still
  // applied when the dns_option is not set.
  if (specifics.dns_option() ==
          sync_pb::WifiConfigurationSpecifics_DnsOption_DNS_OPTION_CUSTOM ||
      (specifics.dns_option() ==
           sync_pb::
               WifiConfigurationSpecifics_DnsOption_DNS_OPTION_UNSPECIFIED &&
       specifics.custom_dns().size())) {
    auto ip_config = network_config::mojom::IPConfigProperties::New();
    std::vector<std::string> custom_dns;
    for (const std::string& nameserver : specifics.custom_dns()) {
      custom_dns.push_back(nameserver);
    }
    ip_config->name_servers = std::move(custom_dns);
    config->static_ip_config = std::move(ip_config);
    config->name_servers_config_type = onc::network_config::kIPConfigTypeStatic;
  } else if (specifics.dns_option() ==
             sync_pb::
                 WifiConfigurationSpecifics_DnsOption_DNS_OPTION_DEFAULT_DHCP) {
    config->name_servers_config_type = onc::network_config::kIPConfigTypeDHCP;
  }

  if (specifics.has_proxy_configuration() &&
      specifics.proxy_configuration().proxy_option() !=
          sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
              PROXY_OPTION_UNSPECIFIED) {
    config->proxy_settings =
        MojoProxySettingsFromProto(specifics.proxy_configuration());
  }

  return config;
}

const NetworkState* NetworkStateFromNetworkIdentifier(
    const NetworkIdentifier& id) {
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::WiFi(), /*configured_only=*/true,
      /*visibleOnly=*/false, /*limit=*/0, &networks);
  for (const NetworkState* network : networks) {
    if (NetworkIdentifier::FromNetworkState(network) == id) {
      return network;
    }
  }
  return nullptr;
}

}  // namespace ash::sync_wifi
