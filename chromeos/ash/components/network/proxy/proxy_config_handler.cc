// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_profile.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "components/onc/onc_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

void NotifyNetworkStateHandler(const std::string& service_path) {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RequestUpdateForNetwork(
        service_path);
  }
}

}  // namespace

namespace proxy_config {

std::unique_ptr<ProxyConfigDictionary> GetProxyConfigForNetwork(
    const PrefService* profile_prefs,
    const PrefService* local_state_prefs,
    const NetworkState& network,
    const NetworkProfileHandler* network_profile_handler,
    ::onc::ONCSource* onc_source) {
  const base::Value::Dict* network_policy = onc::GetPolicyForNetwork(
      profile_prefs, local_state_prefs, network, onc_source);
  if (network_policy) {
    const base::Value::Dict* proxy_policy =
        network_policy->FindDict(::onc::network_config::kProxySettings);
    if (!proxy_policy) {
      // This policy doesn't set a proxy for this network. Nonetheless, this
      // disallows changes by the user.
      return nullptr;
    }

    return std::make_unique<ProxyConfigDictionary>(
        onc::ConvertOncProxySettingsToProxyConfig(*proxy_policy)
            .value_or(base::Value::Dict()));
  }

  if (network.profile_path().empty())
    return nullptr;

  const NetworkProfile* profile =
      network_profile_handler->GetProfileForPath(network.profile_path());
  if (!profile) {
    VLOG(1) << "Unknown profile_path '" << network.profile_path() << "'.";
    return nullptr;
  }
  if (!profile_prefs && profile->type() == NetworkProfile::TYPE_USER) {
    // This case occurs, for example, if called from the proxy config tracker
    // created for the system request context and the signin screen. Both don't
    // use profile prefs and shouldn't depend on the user's not shared proxy
    // settings.
    VLOG(1)
        << "Don't use unshared settings for system context or signin screen.";
    return nullptr;
  }

  // No policy set for this network, read instead the user's (shared or
  // unshared) configuration.
  // The user's proxy setting is not stored in the Chrome preference yet. We
  // still rely on Shill storing it.
  const std::optional<base::Value::Dict>& value = network.proxy_config();
  if (!value) {
    return nullptr;
  }
  return std::make_unique<ProxyConfigDictionary>(value.value().Clone());
}

void SetProxyConfigForNetwork(const ProxyConfigDictionary& proxy_config,
                              const NetworkState& network) {
  // The user's proxy setting is not stored in the Chrome preference yet. We
  // still rely on Shill storing it.
  ProxyPrefs::ProxyMode mode;
  if (!proxy_config.GetMode(&mode) || mode == ProxyPrefs::MODE_DIRECT) {
    // Return empty string for direct mode for portal check to work correctly.
    // TODO(pneubeck): Consider removing this legacy code.
    ShillServiceClient::Get()->ClearProperty(
        dbus::ObjectPath(network.path()), shill::kProxyConfigProperty,
        base::BindOnce(&NotifyNetworkStateHandler, network.path()),
        base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                       "SetProxyConfig.ClearProperty Failed", network.path(),
                       network_handler::ErrorCallback()));
  } else {
    std::string proxy_config_str;
    base::JSONWriter::Write(proxy_config.GetDictionary(), &proxy_config_str);
    ShillServiceClient::Get()->SetProperty(
        dbus::ObjectPath(network.path()), shill::kProxyConfigProperty,
        base::Value(proxy_config_str),
        base::BindOnce(&NotifyNetworkStateHandler, network.path()),
        base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                       "SetProxyConfig.SetProperty Failed", network.path(),
                       network_handler::ErrorCallback()));
  }
}

}  // namespace proxy_config

}  // namespace ash
