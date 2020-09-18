// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "components/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace chromeos {

class NetworkConfigurationHandler;
class NetworkDeviceHandler;
class NetworkPolicyObserver;
class NetworkProfileHandler;
class NetworkStateHandler;

// The ManagedNetworkConfigurationHandler class is used to create and configure
// networks in ChromeOS using ONC and takes care of network policies.
//
// Its interface exposes only ONC and should decouple users from Shill.
// Internally it translates ONC to Shill dictionaries and calls through to the
// NetworkConfigurationHandler.
//
// For accessing lists of visible networks, and other state information, see the
// class NetworkStateHandler.
//
// This is a singleton and its lifetime is managed by the Chrome startup code.
//
// Network configurations are referred to by Shill's service path. These
// identifiers should at most be used to also access network state using the
// NetworkStateHandler, but dependencies to Shill should be avoided. In the
// future, we may switch to other identifiers.
//
// Note on callbacks: Because all the functions here are meant to be
// asynchronous, they all take a |callback| of some type, and an
// |error_callback|. When the operation succeeds, |callback| will be called, and
// when it doesn't, |error_callback| will be called with information about the
// error, including a symbolic name for the error and often some error message
// that is suitable for logging. None of the error message text is meant for
// user consumption.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ManagedNetworkConfigurationHandler {
 public:
  using GuidToPolicyMap =
      std::map<std::string, std::unique_ptr<base::DictionaryValue>>;

  virtual ~ManagedNetworkConfigurationHandler();

  virtual void AddObserver(NetworkPolicyObserver* observer) = 0;
  virtual void RemoveObserver(NetworkPolicyObserver* observer) = 0;

  // Provides the properties of the network with |service_path| to |callback|.
  // |userhash| is used to set the "Source" property. If not provided then
  // user policies will be ignored.
  virtual void GetProperties(const std::string& userhash,
                             const std::string& service_path,
                             network_handler::PropertiesCallback callback) = 0;

  // Provides the managed properties of the network with |service_path| to
  // |callback|. |userhash| is used to ensure that the user's policy is
  // already applied, and to set the "Source" property (see note for
  // GetProperties).
  virtual void GetManagedProperties(
      const std::string& userhash,
      const std::string& service_path,
      network_handler::PropertiesCallback callback) = 0;

  // Sets the user's settings of an already configured network with
  // |service_path|. A network can be initially configured by calling
  // CreateConfiguration or if it is managed by a policy. The given properties
  // will be merged with the existing settings, and it won't clear any existing
  // properties.
  virtual void SetProperties(const std::string& service_path,
                             const base::DictionaryValue& user_settings,
                             base::OnceClosure callback,
                             network_handler::ErrorCallback error_callback) = 0;

  // Initially configures an unconfigured network with the given user settings
  // and returns the new identifier to |callback| if successful. Fails if the
  // network was already configured by a call to this function or because of a
  // policy. The new configuration will be owned by user |userhash|. If
  // |userhash| is empty, the new configuration will be shared.
  virtual void CreateConfiguration(
      const std::string& userhash,
      const base::DictionaryValue& properties,
      network_handler::ServiceResultCallback callback,
      network_handler::ErrorCallback error_callback) const = 0;

  // Removes the user's configuration from the network with |service_path|. The
  // network may still show up in the visible networks after this, but no user
  // configuration will remain. If it was managed, it will still be configured.
  virtual void RemoveConfiguration(
      const std::string& service_path,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) const = 0;

  // Removes the user's configuration from the network with |service_path| in
  // the network's active network profile.
  // Same applies as for |RemoveConfiguration|, with the difference that the
  // configuration is only removed from a single network profile.
  virtual void RemoveConfigurationFromCurrentProfile(
      const std::string& service_path,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) const = 0;

  // Only to be called by NetworkConfigurationUpdater or from tests. Sets
  // |network_configs_onc| and |global_network_config| as the current policy of
  // |userhash| and |onc_source|. The policy will be applied (not necessarily
  // immediately) to Shill's profiles and enforced in future configurations
  // until the policy associated with |userhash| and |onc_source| is changed
  // again with this function. For device policies, |userhash| must be empty.
  virtual void SetPolicy(
      ::onc::ONCSource onc_source,
      const std::string& userhash,
      const base::ListValue& network_configs_onc,
      const base::DictionaryValue& global_network_config) = 0;

  // Returns true if any policy application is currently running or pending.
  // NetworkPolicyObservers are notified about applications finishing.
  virtual bool IsAnyPolicyApplicationRunning() const = 0;

  // Returns the user policy for user |userhash| or device policy, which has
  // |guid|. If |userhash| is empty, only looks for a device policy. If such
  // doesn't exist, returns NULL. Sets |onc_source| accordingly.
  virtual const base::DictionaryValue* FindPolicyByGUID(
      const std::string userhash,
      const std::string& guid,
      ::onc::ONCSource* onc_source) const = 0;

  virtual const GuidToPolicyMap* GetNetworkConfigsFromPolicy(
      const std::string& userhash) const = 0;

  // Returns the global configuration of the policy of user |userhash| or device
  // policy if |userhash| is empty.
  virtual const base::DictionaryValue* GetGlobalConfigFromPolicy(
      const std::string& userhash) const = 0;

  // Returns the policy with |guid| for profile |profile_path|. If such
  // doesn't exist, returns nullptr. Sets |onc_source| accordingly if it is not
  // nullptr.
  virtual const base::DictionaryValue* FindPolicyByGuidAndProfile(
      const std::string& guid,
      const std::string& profile_path,
      ::onc::ONCSource* onc_source) const = 0;

  // Returns true if the network with |guid| is configured by device or user
  // policy for profile |profile_path|.
  virtual bool IsNetworkConfiguredByPolicy(
      const std::string& guid,
      const std::string& profile_path) const = 0;

  // Returns true if the configuration of the network with |guid| is not
  // managed by policy for profile with |profile_path| and thus can be removed.
  virtual bool CanRemoveNetworkConfig(
      const std::string& guid,
      const std::string& profile_path) const = 0;

  // Return true if the AllowOnlyPolicyNetworksToConnect policy is enabled.
  virtual bool AllowOnlyPolicyNetworksToConnect() const = 0;

  // Return true if the AllowOnlyPolicyNetworksToConnectIfAvailable policy is
  // enabled.
  virtual bool AllowOnlyPolicyNetworksToConnectIfAvailable() const = 0;

  // Return true if the AllowOnlyPolicyNetworksToAutoconnect policy is enabled.
  virtual bool AllowOnlyPolicyNetworksToAutoconnect() const = 0;

  // Return the list of blocked WiFi networks (identified by HexSSIDs).
  virtual std::vector<std::string> GetBlockedHexSSIDs() const = 0;

  static std::unique_ptr<ManagedNetworkConfigurationHandler>
  InitializeForTesting(
      NetworkStateHandler* network_state_handler,
      NetworkProfileHandler* network_profile_handler,
      NetworkDeviceHandler* network_device_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      UIProxyConfigService* ui_proxy_config_service);

 private:
  DISALLOW_ASSIGN(ManagedNetworkConfigurationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
