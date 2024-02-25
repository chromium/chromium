// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/tether/wifi_hotspot_disconnector.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

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

  WifiHotspotDisconnectorImpl(const WifiHotspotDisconnectorImpl&) = delete;
  WifiHotspotDisconnectorImpl& operator=(const WifiHotspotDisconnectorImpl&) =
      delete;

  ~WifiHotspotDisconnectorImpl() override;

  // WifiHotspotDisconnector:
  void DisconnectFromWifiHotspot(const std::string& wifi_network_guid,
                                 base::OnceClosure success_callback,
                                 StringErrorCallback error_callback) override;

 private:
  void OnSuccessfulWifiDisconnect(const std::string& wifi_network_guid,
                                  const std::string& wifi_network_path,
                                  base::OnceClosure success_callback,
                                  StringErrorCallback error_callback);
  void OnFailedWifiDisconnect(const std::string& wifi_network_guid,
                              const std::string& wifi_network_path,
                              StringErrorCallback error_callback,
                              const std::string& error_name);
  void CleanUpAfterWifiDisconnection(const std::string& wifi_network_path,
                                     base::OnceClosure success_callback,
                                     StringErrorCallback error_callback);

  raw_ptr<NetworkConnectionHandler> network_connection_handler_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<NetworkConfigurationRemover> network_configuration_remover_;

  base::WeakPtrFactory<WifiHotspotDisconnectorImpl> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_IMPL_H_
