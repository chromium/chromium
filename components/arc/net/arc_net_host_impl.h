// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
#define COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/arc/mojom/net.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

class PrefService;

namespace arc {

class ArcBridgeService;

// Private implementation of ArcNetHost.
class ArcNetHostImpl : public KeyedService,
                       public ConnectionObserver<mojom::NetInstance>,
                       public chromeos::NetworkConnectionObserver,
                       public chromeos::NetworkStateHandlerObserver,
                       public mojom::NetHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcNetHostImpl* GetForBrowserContext(content::BrowserContext* context);
  static ArcNetHostImpl* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // The constructor will register an Observer with ArcBridgeService.
  ArcNetHostImpl(content::BrowserContext* context,
                 ArcBridgeService* arc_bridge_service);
  ~ArcNetHostImpl() override;

  void SetPrefService(PrefService* pref_service);

  // ARC -> Chrome calls:

  void GetNetworks(mojom::GetNetworksRequestType type,
                   GetNetworksCallback callback) override;

  void GetWifiEnabledState(GetWifiEnabledStateCallback callback) override;

  void SetWifiEnabledState(bool is_enabled,
                           SetWifiEnabledStateCallback callback) override;

  void StartScan() override;

  void CreateNetwork(mojom::WifiConfigurationPtr cfg,
                     CreateNetworkCallback callback) override;

  void ForgetNetwork(const std::string& guid,
                     ForgetNetworkCallback callback) override;

  void StartConnect(const std::string& guid,
                    StartConnectCallback callback) override;

  void StartDisconnect(const std::string& guid,
                       StartDisconnectCallback callback) override;

  void AndroidVpnConnected(mojom::AndroidVpnConfigurationPtr cfg) override;

  void AndroidVpnStateChanged(mojom::ConnectionStateType state) override;

  void SetAlwaysOnVpn(const std::string& vpnPackage, bool lockdown) override;

  std::unique_ptr<base::DictionaryValue> TranslateVpnConfigurationToOnc(
      const mojom::AndroidVpnConfiguration& cfg);

  // Overriden from chromeos::NetworkStateHandlerObserver.
  void ScanCompleted(const chromeos::DeviceState* /*unused*/) override;
  void OnShuttingDown() override;
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;
  void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) override;
  void ActiveNetworksChanged(
      const std::vector<const chromeos::NetworkState*>& networks) override;
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void GetDefaultNetwork(GetDefaultNetworkCallback callback) override;

  // Overriden from chromeos::NetworkConnectionObserver.
  void DisconnectRequested(const std::string& service_path) override;

  // Overridden from ConnectionObserver<mojom::NetInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

 private:
  const chromeos::NetworkState* GetDefaultNetworkFromChrome();
  void UpdateDefaultNetwork();
  void DefaultNetworkSuccessCallback(const std::string& service_path,
                                     const base::DictionaryValue& dictionary);

  // Due to a race in Chrome, GetNetworkStateFromGuid() might not know about
  // newly created networks, as that function relies on the completion of a
  // separate GetProperties shill call that completes asynchronously.  So this
  // class keeps a local cache of the path->guid mapping as a fallback.
  // This is sufficient to pass CTS but it might not handle multiple
  // successive Create operations (crbug.com/631646).
  bool GetNetworkPathFromGuid(const std::string& guid, std::string* path);

  // Look through the list of known networks for an ARC VPN service.
  // If found, return the Shill service path.  Otherwise return
  // an empty string.  It is assumed that there is at most one ARC VPN
  // service in the list, as the same service will be reused for every
  // ARC VPN connection.
  std::string LookupArcVpnServicePath();

  // Convert a vector of strings, |string_list|, to a base::Value
  // that can be added to an ONC dictionary.  This is used for fields
  // like NameServers, SearchDomains, etc.
  std::unique_ptr<base::Value> TranslateStringListToValue(
      const std::vector<std::string>& string_list);

  // Ask Shill to connect to the Android VPN with name |service_path|.
  // |service_path| and |guid| are stored locally for future reference.
  // This is used as the callback from a CreateConfiguration() or
  // SetProperties() call, depending on whether an ARCVPN service already
  // exists.
  void ConnectArcVpn(const std::string& service_path, const std::string& guid);

  // Ask Android to disconnect any VPN app that is currently connected.
  void DisconnectArcVpn();

  void CreateNetworkSuccessCallback(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& service_path,
      const std::string& guid);

  void CreateNetworkFailureCallback(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // True if the chrome::NetworkStateHandler is currently being observed for
  // state changes.
  bool observing_network_state_ = false;

  std::string cached_service_path_;
  std::string cached_guid_;
  std::string arc_vpn_service_path_;
  // Owned by the user profile whose context was used to initialize |this|.
  PrefService* pref_service_ = nullptr;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ArcNetHostImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcNetHostImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
