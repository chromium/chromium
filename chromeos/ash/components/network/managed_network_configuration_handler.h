// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "components/onc/onc_constants.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

class NetworkConfigurationHandler;
class NetworkDeviceHandler;
class NetworkPolicyObserver;
struct NetworkProfile;
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
  // Specifies which policy type a caller is interested in.
  enum class PolicyType {
    // Original ONC policy as provided by cloud policy.
    kOriginal,
    // ONC policy with runtime values set, i.e. variables can be expanded and a
    // resolved client certificate set.
    kWithRuntimeValues,
  };

  ManagedNetworkConfigurationHandler& operator=(
      const ManagedNetworkConfigurationHandler&) = delete;

  virtual ~ManagedNetworkConfigurationHandler();

  virtual void AddObserver(NetworkPolicyObserver* observer) = 0;
  virtual void RemoveObserver(NetworkPolicyObserver* observer) = 0;
  virtual bool HasObserver(NetworkPolicyObserver* observer) const = 0;

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
                             const base::Value::Dict& user_settings,
                             base::OnceClosure callback,
                             network_handler::ErrorCallback error_callback) = 0;

  // Clears Shill properties in |names| of a network with |service_path|.
  virtual void ClearShillProperties(
      const std::string& service_path,
      const std::vector<std::string>& names,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) = 0;

  // Initially configures an unconfigured network with the given user settings
  // and returns the new identifier to |callback| if successful. Fails if the
  // network was already configured by a call to this function or because of a
  // policy. The new configuration will be owned by user |userhash|. If
  // |userhash| is empty, the new configuration will be shared.
  virtual void CreateConfiguration(
      const std::string& userhash,
      const base::Value::Dict& properties,
      network_handler::ServiceResultCallback callback,
      network_handler::ErrorCallback error_callback) const = 0;

  // Creates network configuration with given |shill_properties| from policy.
  // Any conflicting configuration for the same network will have to be removed
  // before calling this method. |callback| will be called after the
  // configuration update has been reflected in NetworkStateHandler, or on
  // error. This fires OnPolicyApplied notification on success.
  virtual void ConfigurePolicyNetwork(const base::Value::Dict& shill_properties,
                                      base::OnceClosure callback) const = 0;

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
  virtual void SetPolicy(::onc::ONCSource onc_source,
                         const std::string& userhash,
                         const base::Value::List& network_configs_onc,
                         const base::Value::Dict& global_network_config) = 0;

  // Returns true if any policy application is currently running or pending.
  // NetworkPolicyObservers are notified about applications finishing.
  virtual bool IsAnyPolicyApplicationRunning() const = 0;

  // Sets ONC variable expansions for |userhash|.
  // These expansions are profile-wide, i.e. they will apply to all networks
  // that belong to |userhash|.
  // This overwrites any previously-set profile-wide variable expansions.
  // If this call changes the effective ONC policy (after variable expansion) of
  // any network config, it triggers re-application of that network policy.
  virtual void SetProfileWideVariableExpansions(
      const std::string& userhash,
      base::flat_map<std::string, std::string> expansions) = 0;

  // Sets the resolved certificate for the network |guid|.
  // Returns true if this resulted in an effective change.
  virtual bool SetResolvedClientCertificate(
      const std::string& userhash,
      const std::string& guid,
      client_cert::ResolvedCert resolved_cert) = 0;

  // Returns the user policy for user |userhash| or device policy, which has
  // |guid|. If |userhash| is empty, only looks for a device policy. If such
  // doesn't exist, returns NULL. Sets |onc_source| accordingly.
  virtual const base::Value::Dict* FindPolicyByGUID(
      const std::string userhash,
      const std::string& guid,
      ::onc::ONCSource* onc_source) const = 0;

  // Calls GetProperties and runs ResetDNSPropertiesCallback as the primary
  // callback, changes the NameServersConfigType ONC property to be
  // automatically set by DHCP and applies it to a specific network device.
  virtual void ResetDNSProperties(const std::string& service_path) = 0;

  // Returns true if the user policy for |userhash| or device policy if
  // |userhash| is empty has any policy-configured network.
  // Returns false if |userhash| does not map to any known network profile.
  virtual bool HasAnyPolicyNetwork(const std::string& userhash) const = 0;

  // Returns the global configuration of the policy of user |userhash| or device
  // policy if |userhash| is empty.
  virtual const base::Value::Dict* GetGlobalConfigFromPolicy(
      const std::string& userhash) const = 0;

  // Returns the policy with |guid| for profile |profile_path|. If such
  // doesn't exist, returns nullptr. Sets |onc_source| and |userhash|
  // accordingly if it is not nullptr.
  virtual const base::Value::Dict* FindPolicyByGuidAndProfile(
      const std::string& guid,
      const std::string& profile_path,
      PolicyType policy_type,
      ::onc::ONCSource* out_onc_source,
      std::string* out_userhash) const = 0;

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

  // Notify observers that the policy has been fully applied and is reflected in
  // NetworkStateHandler.
  virtual void NotifyPolicyAppliedToNetwork(
      const std::string& service_path) const = 0;

  // Called after new Cellular networks have been provisioned and configured via
  // policy. CellularPolicyHandler calls this method after eSIM profiles are
  // installed from policy. The network list should be updated at this point.
  virtual void OnCellularPoliciesApplied(const NetworkProfile& profile) = 0;

  // Triggers performing tasks to wipe network configuration elements marked as
  // ephemeral by device policy.
  virtual void TriggerEphemeralNetworkConfigActions() = 0;

  // Return true if AllowAPNModification policy is enabled.
  virtual bool AllowApnModification() const = 0;

  // Return true if AllowCellularSimLock policy is enabled.
  virtual bool AllowCellularSimLock() const = 0;

  // Return true if AllowCellularHotspot policy is enabled.
  virtual bool AllowCellularHotspot() const = 0;

  // Return true if AllowOnlyPolicyCellularNetworks policy is enabled.
  virtual bool AllowOnlyPolicyCellularNetworks() const = 0;

  // Return true if the AllowOnlyPolicyWiFiToConnect policy is enabled.
  virtual bool AllowOnlyPolicyWiFiToConnect() const = 0;

  // Return true if the AllowOnlyPolicyWiFiToConnectIfAvailable policy is
  // enabled.
  virtual bool AllowOnlyPolicyWiFiToConnectIfAvailable() const = 0;

  // Return true if the AllowOnlyPolicyNetworksToAutoconnect policy is enabled.
  virtual bool AllowOnlyPolicyNetworksToAutoconnect() const = 0;

  // Return true if the RecommendedValuesAreEphemeral policy is enabled.
  virtual bool RecommendedValuesAreEphemeral() const = 0;

  // Return true if the UserCreatedNetworkConfigurationsAreEphemeral policy is
  // enabled.
  virtual bool UserCreatedNetworkConfigurationsAreEphemeral() const = 0;

  // Return true if the following user prefs exist and meet the following
  // conditions: `arc::prefs::kAlwaysOnVpnPackage` is non-empty,
  // `arc::prefs::kAlwaysOnVpnLockdown` is true, and `prefs::kVpnConfigAllowed`
  // is false.
  virtual bool IsProhibitedFromConfiguringVpn() const = 0;

  // Returns the value for the AllowTextMessages policy.
  virtual PolicyTextMessageSuppressionState GetAllowTextMessages() const = 0;

  // Return the list of blocked WiFi networks (identified by HexSSIDs).
  virtual std::vector<std::string> GetBlockedHexSSIDs() const = 0;

  // Called after either secure DNS status or deviceReportXDREvents policy is
  // updated.
  virtual void OnEnterpriseMonitoredWebPoliciesApplied() const = 0;

  // Called just before destruction to give observers a chance to remove
  // themselves and disable any networking.
  virtual void Shutdown() = 0;

  static std::unique_ptr<ManagedNetworkConfigurationHandler>
  InitializeForTesting(
      NetworkStateHandler* network_state_handler,
      NetworkProfileHandler* network_profile_handler,
      NetworkDeviceHandler* network_device_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      UIProxyConfigService* ui_proxy_config_service);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
