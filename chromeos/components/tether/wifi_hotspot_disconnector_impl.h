// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/wifi_hotspot_disconnector.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class NetworkConnectionHandler;
class NetworkStateHandler;

namespace tether {

class NetworkConfigurationRemover;

// Concrete WifiHotspotDisconnector implementation.
class WifiHotspotDisconnectorImpl : public WifiHotspotDisconnector {
 public:
  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  WifiHotspotDisconnectorImpl(
      NetworkConnectionHandler* network_connection_handler,
      NetworkStateHandler* network_state_handler,
      PrefService* pref_service,
      NetworkConfigurationRemover* network_configuration_remover);
  ~WifiHotspotDisconnectorImpl() override;

  // WifiHotspotDisconnector:
  void DisconnectFromWifiHotspot(
      const std::string& wifi_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override;

 private:
  void OnSuccessfulWifiDisconnect(
      const std::string& wifi_network_guid,
      const std::string& wifi_network_path,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback);
  void OnFailedWifiDisconnect(
      const std::string& wifi_network_guid,
      const std::string& wifi_network_path,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);
  void CleanUpAfterWifiDisconnection(
      bool success,
      const std::string& wifi_network_path,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback);

  NetworkConnectionHandler* network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  PrefService* pref_service_;
  NetworkConfigurationRemover* network_configuration_remover_;

  base::WeakPtrFactory<WifiHotspotDisconnectorImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WifiHotspotDisconnectorImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_IMPL_H_
