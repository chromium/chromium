// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile_observer.h"
#include "chromeos/network/policy_applicator.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace chromeos {

class NetworkConfigurationHandler;
struct NetworkProfile;
class NetworkProfileHandler;
class NetworkStateHandler;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) ManagedNetworkConfigurationHandlerImpl
    : public ManagedNetworkConfigurationHandler,
      public NetworkProfileObserver,
      public PolicyApplicator::ConfigurationHandler {
 public:
  ~ManagedNetworkConfigurationHandlerImpl() override;

  // ManagedNetworkConfigurationHandler overrides
  void AddObserver(NetworkPolicyObserver* observer) override;
  void RemoveObserver(NetworkPolicyObserver* observer) override;

  void GetProperties(const std::string& userhash,
                     const std::string& service_path,
                     network_handler::PropertiesCallback callback) override;

  void GetManagedProperties(
      const std::string& userhash,
      const std::string& service_path,
      network_handler::PropertiesCallback callback) override;

  void SetProperties(const std::string& service_path,
                     const base::DictionaryValue& user_settings,
                     base::OnceClosure callback,
                     network_handler::ErrorCallback error_callback) override;

  void CreateConfiguration(
      const std::string& userhash,
      const base::DictionaryValue& properties,
      network_handler::ServiceResultCallback callback,
      network_handler::ErrorCallback error_callback) const override;

  void RemoveConfiguration(
      const std::string& service_path,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) const override;

  void RemoveConfigurationFromCurrentProfile(
      const std::string& service_path,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) const override;

  void SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 const base::ListValue& network_configs_onc,
                 const base::DictionaryValue& global_network_config) override;

  bool IsAnyPolicyApplicationRunning() const override;

  const base::DictionaryValue* FindPolicyByGUID(
      const std::string userhash,
      const std::string& guid,
      ::onc::ONCSource* onc_source) const override;

  const GuidToPolicyMap* GetNetworkConfigsFromPolicy(
      const std::string& userhash) const override;

  const base::DictionaryValue* GetGlobalConfigFromPolicy(
      const std::string& userhash) const override;

  const base::DictionaryValue* FindPolicyByGuidAndProfile(
      const std::string& guid,
      const std::string& profile_path,
      ::onc::ONCSource* onc_source) const override;

  bool IsNetworkConfiguredByPolicy(
      const std::string& guid,
      const std::string& profile_path) const override;

  bool CanRemoveNetworkConfig(const std::string& guid,
                              const std::string& profile_path) const override;

  bool AllowOnlyPolicyNetworksToConnect() const override;
  bool AllowOnlyPolicyNetworksToConnectIfAvailable() const override;
  bool AllowOnlyPolicyNetworksToAutoconnect() const override;
  std::vector<std::string> GetBlockedHexSSIDs() const override;

  // NetworkProfileObserver overrides
  void OnProfileAdded(const NetworkProfile& profile) override;
  void OnProfileRemoved(const NetworkProfile& profile) override;

  // PolicyApplicator::ConfigurationHandler overrides
  void CreateConfigurationFromPolicy(
      const base::DictionaryValue& shill_properties,
      base::OnceClosure callback) override;

  void UpdateExistingConfigurationWithPropertiesFromPolicy(
      const base::DictionaryValue& existing_properties,
      const base::DictionaryValue& new_properties,
      base::OnceClosure callback) override;

  void OnPoliciesApplied(const NetworkProfile& profile) override;

 private:
  friend class AutoConnectHandlerTest;
  friend class ClientCertResolverTest;
  friend class ManagedNetworkConfigurationHandler;
  friend class ManagedNetworkConfigurationHandlerTest;
  friend class ManagedNetworkConfigurationHandlerMockTest;
  friend class NetworkConnectionHandlerImplTest;
  friend class NetworkHandler;
  friend class ProhibitedTechnologiesHandlerTest;

  struct Policies;
  typedef std::map<std::string, std::unique_ptr<Policies>> UserToPoliciesMap;
  typedef std::map<std::string, std::unique_ptr<PolicyApplicator>>
      UserToPolicyApplicatorMap;
  typedef std::map<std::string, std::set<std::string>>
      UserToModifiedPoliciesMap;

  // The type of properties to send after a Get{Managed}Properties call.
  enum class PropertiesType {
    kUnmanaged,
    kManaged,
  };

  ManagedNetworkConfigurationHandlerImpl();

  // Handlers may be null in tests so long as they do not execute any paths
  // that require the handlers.
  void Init(NetworkStateHandler* network_state_handler,
            NetworkProfileHandler* network_profile_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            NetworkDeviceHandler* network_device_handler,
            ProhibitedTechnologiesHandler* prohibitied_technologies_handler);

  // Returns the Policies for the given |userhash|, or the device policies if
  // |userhash| is empty.
  const Policies* GetPoliciesForUser(const std::string& userhash) const;
  // Returns the Policies for the given network |profile|. These could be either
  // user or device policies.
  const Policies* GetPoliciesForProfile(const NetworkProfile& profile) const;

  // Called when a policy identified by |guid| has been applied to the network
  // identified by |service_path|. Notifies observers and calls |callback|.
  void OnPolicyAppliedToNetwork(base::OnceClosure callback,
                                const std::string& service_path,
                                const std::string& guid);

  // Helper method to append associated Device properties to |properties|.
  void GetDeviceStateProperties(const std::string& service_path,
                                base::Value* properties);

  // Callback for NetworkConfigurationHandler::GetProperties requests from
  // Get{Managed}Properties. This callback fills in properties from
  // DeviceState and may request additional Device properties.
  // Note: Requesting Device properties requires an additional fetch and
  // additional copying of data, so we only do it for Cellular networks which
  // contain a lot of necessary state in the associated Device object.
  void GetPropertiesCallback(PropertiesType properties_type,
                             const std::string& userhash,
                             network_handler::PropertiesCallback callback,
                             const std::string& service_path,
                             base::Optional<base::Value> shill_properties);

  void OnGetDeviceProperties(PropertiesType properties_type,
                             const std::string& userhash,
                             const std::string& service_path,
                             network_handler::PropertiesCallback callback,
                             base::Optional<base::Value> network_properties,
                             const std::string& device_path,
                             base::Optional<base::Value> device_properties);

  void SendProperties(PropertiesType properties_type,
                      const std::string& userhash,
                      const std::string& service_path,
                      network_handler::PropertiesCallback callback,
                      base::Optional<base::Value> shill_properties);

  // Called from SetProperties, calls NCH::SetShillProperties.
  void SetShillProperties(
      const std::string& service_path,
      std::unique_ptr<base::DictionaryValue> shill_dictionary,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback);

  // Sets the active proxy values in managed network configurations depending on
  // the source of the configuration. Proxy enforced by user policy
  // (provided by kProxy prefence) should have precedence over configurations
  // set by ONC policy.
  void SetManagedActiveProxyValues(const std::string& guid,
                                   base::DictionaryValue* dictionary);

  // Applies policies for |userhash|. |modified_policies| must be not null and
  // contain the GUIDs of the network configurations that changed since the last
  // policy application. Returns true if policy application was started and
  // false if it was queued or delayed.
  bool ApplyOrQueuePolicies(const std::string& userhash,
                            std::set<std::string>* modified_policies);

  void set_ui_proxy_config_service(
      UIProxyConfigService* ui_proxy_config_service);

  // If present, the empty string maps to the device policy.
  UserToPoliciesMap policies_by_user_;

  // Local references to the associated handler instances.
  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkProfileHandler* network_profile_handler_ = nullptr;
  NetworkConfigurationHandler* network_configuration_handler_ = nullptr;
  NetworkDeviceHandler* network_device_handler_ = nullptr;
  ProhibitedTechnologiesHandler* prohibited_technologies_handler_ = nullptr;
  UIProxyConfigService* ui_proxy_config_service_ = nullptr;

  // Owns the currently running PolicyApplicators.
  UserToPolicyApplicatorMap policy_applicators_;

  // Per userhash (or empty string for device policy), contains the GUIDs of the
  // policies that were modified.
  // If this map contains a userhash as key, it means that a policy application
  // for this userhash is pending even if no policies were modified and the
  // associated set of GUIDs is empty.
  UserToModifiedPoliciesMap queued_modified_policies_;

  base::ObserverList<NetworkPolicyObserver, true>::Unchecked observers_;

  bool user_policy_applied_ = false;
  bool device_policy_applied_ = false;

  // For Shill client callbacks
  base::WeakPtrFactory<ManagedNetworkConfigurationHandlerImpl>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
