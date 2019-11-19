// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PROXY_PROXY_CONFIG_SERVICE_IMPL_H_
#define CHROMEOS_NETWORK_PROXY_PROXY_CONFIG_SERVICE_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

namespace chromeos {

class NetworkState;

// Implementation of proxy config service for chromeos that:
// - extends PrefProxyConfigTrackerImpl (and so lives and runs entirely on UI
//   thread) to handle proxy from prefs (via PrefProxyConfigTrackerImpl) and
//   system i.e. network (via shill notifications)
// - exists one per profile and one per local state
// - persists proxy setting per network in flimflim
// - provides network stack with latest effective proxy configuration for
//   currently active network via PrefProxyConfigTrackerImpl's mechanism of
//   pushing config to ChromeProxyConfigService
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ProxyConfigServiceImpl
    : public PrefProxyConfigTrackerImpl,
      public NetworkStateHandlerObserver {
 public:
  // ProxyConfigServiceImpl is created in ProxyServiceFactory::
  // CreatePrefProxyConfigTrackerImpl via Profile::GetProxyConfigTracker() for
  // profile or via IOThread constructor for local state and is owned by the
  // respective classes.
  //
  // The user's proxy config, proxy policies and proxy from prefs, are used to
  // determine the effective proxy config, which is then pushed through
  // PrefProxyConfigTrackerImpl to ChromeProxyConfigService to the
  // network stack.
  //
  // |profile_prefs| can be NULL if this object should only track prefs from
  // local state (e.g., for system request context or sigin-in screen).
  ProxyConfigServiceImpl(
      PrefService* profile_prefs,
      PrefService* local_state_prefs,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~ProxyConfigServiceImpl() override;

  // PrefProxyConfigTrackerImpl implementation.
  void OnProxyConfigChanged(
      ProxyPrefs::ConfigState config_state,
      const net::ProxyConfigWithAnnotation& config) override;

  // NetworkStateHandlerObserver implementation.
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // Returns true if proxy is to be ignored for this network profile and
  // |onc_source|, e.g. this happens if the network is shared and
  // use-shared-proxies is turned off. |profile_prefs| may be NULL.
  static bool IgnoreProxy(const PrefService* profile_prefs,
                          const std::string network_profile_path,
                          ::onc::ONCSource onc_source);

  // Returns Pref Proxy configuration if available or a proxy config dictionary
  // applied to the default network.
  // Returns null if no Pref Proxy configuration and no active network.
  // |profile_prefs| and |local_state_prefs| must be not null.
  static std::unique_ptr<ProxyConfigDictionary> GetActiveProxyConfigDictionary(
      const PrefService* profile_prefs,
      const PrefService* local_state_prefs);

 private:
  // Called when any proxy preference changes.
  void OnProxyPrefChanged();

  // Determines effective proxy config based on prefs from config tracker, the
  // current default network and if user is using shared proxies.  The effective
  // config is stored in |active_config_| and activated on network stack, and
  // hence, picked up by observers.
  void DetermineEffectiveConfigFromDefaultNetwork();

  // Track changes in profile preferences: UseSharedProxies and
  // OpenNetworkConfiguration.
  PrefChangeRegistrar profile_pref_registrar_;

  // Track changes in local state preferences: DeviceOpenNetworkConfiguration.
  PrefChangeRegistrar local_state_pref_registrar_;

  // Not owned. NULL if tracking only local state prefs (e.g. in the system
  // request context or sign-in screen).
  PrefService* profile_prefs_;

  // Not owned.
  PrefService* local_state_prefs_;

  base::WeakPtrFactory<ProxyConfigServiceImpl> pointer_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_PROXY_PROXY_CONFIG_SERVICE_IMPL_H_
