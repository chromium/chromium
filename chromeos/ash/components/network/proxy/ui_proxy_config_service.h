// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PROXY_UI_PROXY_CONFIG_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PROXY_UI_PROXY_CONFIG_SERVICE_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/proxy_config/proxy_prefs.h"

class PrefService;

namespace ash {

class NetworkProfileHandler;
class NetworkState;
class NetworkStateHandler;

// This class provides an interface to the UI for getting a network proxy
// configuration.
// NOTE: This class must be rebuilt with the primary user's profile prefs when
// the primary user logs in.
// ALSO NOTE: The provided PrefService instances are used both to retrieve proxy
// configurations set by an extension, and for ONC policy information associated
// with a network. (Per-network proxy configurations are stored in Shill,
// but ONC policy configuration is stored in PrefService).
class COMPONENT_EXPORT(CHROMEOS_NETWORK) UIProxyConfigService {
 public:
  // |local_state_prefs| must not be null. |profile_prefs| can be
  // null if there is no logged in user, in which case only the local state
  // (device) prefs will be used. See note above.
  UIProxyConfigService(PrefService* profile_prefs,
                       PrefService* local_state_prefs,
                       NetworkStateHandler* network_state_handler,
                       NetworkProfileHandler* network_profile_handler);

  UIProxyConfigService(const UIProxyConfigService&) = delete;
  UIProxyConfigService& operator=(const UIProxyConfigService&) = delete;

  ~UIProxyConfigService();

  // Generates ONC dictionary for proxy settings enforced for the network, and
  // writes them to |proxy_settings|. The proxy settings that will be written to
  // |proxy_settings| will be one of the following (in order of preference):
  // * A proxy enforced by a user policy (provided by kProxy prefence).
  // * A proxy set by an extension in the active PrefService (also provided by
  //   kProxy pref).
  // * A proxy set by an ONC policy associated with |network_guid|.
  //
  // |proxy_settings| is expected to be a dictionary value containing ONC proxy
  // settings, and will generally contain the proxy settings reported by shill
  // (which will have user set per-network proxy settings, if they are
  // available).
  //
  // Returns whether |proxy_settings| have been changed.
  bool MergeEnforcedProxyConfig(const std::string& network_guid,
                                base::Value::Dict* proxy_settings);

  // Returns the ProxyMode for |network|. The returned result is used to display
  // a privacy warning to the user in the system tray.
  ProxyPrefs::ProxyMode ProxyModeForNetwork(const NetworkState* network);

 private:
  void OnPreferenceChanged(const std::string& pref_name);

  // GUID of network used for current_ui_config_.
  std::string current_ui_network_guid_;

  raw_ptr<PrefService> profile_prefs_;  // unowned
  PrefChangeRegistrar profile_registrar_;

  raw_ptr<PrefService> local_state_prefs_;  // unowned
  PrefChangeRegistrar local_state_registrar_;

  raw_ptr<NetworkStateHandler, DanglingUntriaged>
      network_state_handler_;  // unowned
  raw_ptr<NetworkProfileHandler, DanglingUntriaged>
      network_profile_handler_;  // unowned
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PROXY_UI_PROXY_CONFIG_SERVICE_H_
