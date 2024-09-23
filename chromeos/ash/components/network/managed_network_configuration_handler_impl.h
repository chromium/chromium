// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_profile_observer.h"
#include "chromeos/ash/components/network/policy_applicator.h"
#include "chromeos/ash/components/network/profile_policies.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "components/prefs/pref_service.h"

class PrefService;

namespace base {
class Value;
}  // namespace base

namespace ash {

class CellularPolicyHandler;
class ManagedCellularPrefHandler;
class NetworkConfigurationHandler;
struct NetworkProfile;
class NetworkProfileHandler;
class NetworkStateHandler;
class HotspotController;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) ManagedNetworkConfigurationHandlerImpl
    : public ManagedNetworkConfigurationHandler,
      public NetworkProfileObserver,
      public PolicyApplicator::ConfigurationHandler {
 public:
  ManagedNetworkConfigurationHandlerImpl(
      const ManagedNetworkConfigurationHandlerImpl&) = delete;
  ManagedNetworkConfigurationHandlerImpl& operator=(
      const ManagedNetworkConfigurationHandlerImpl&) = delete;

  ~ManagedNetworkConfigurationHandlerImpl() override;

  // ManagedNetworkConfigurationHandler overrides
  void AddObserver(NetworkPolicyObserver* observer) override;
  void RemoveObserver(NetworkPolicyObserver* observer) override;
  bool HasObserver(NetworkPolicyObserver* observer) const override;

  void GetProperties(const std::string& userhash,
                     const std::string& service_path,
                     network_handler::PropertiesCallback callback) override;

  void GetManagedProperties(
      const std::string& userhash,
      const std::string& service_path,
      network_handler::PropertiesCallback callback) override;

  void SetProperties(const std::string& service_path,
                     const base::Value::Dict& user_settings,
                     base::OnceClosure callback,
                     network_handler::ErrorCallback error_callback) override;

  void ClearShillProperties(
      const std::string& service_path,
      const std::vector<std::string>& names,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) override;

  void CreateConfiguration(
      const std::string& userhash,
      const base::Value::Dict& properties,
      network_handler::ServiceResultCallback callback,
      network_handler::ErrorCallback error_callback) const override;

  void ConfigurePolicyNetwork(const base::Value::Dict& shill_properties,
                              base::OnceClosure callback) const override;

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
                 const base::Value::List& network_configs_onc,
                 const base::Value::Dict& global_network_config) override;

  bool IsAnyPolicyApplicationRunning() const override;

  void SetProfileWideVariableExpansions(
      const std::string& userhash,
      base::flat_map<std::string, std::string> expansions) override;

  bool SetResolvedClientCertificate(
      const std::string& userhash,
      const std::string& guid,
      client_cert::ResolvedCert resolved_cert) override;

  const base::Value::Dict* FindPolicyByGUID(
      const std::string userhash,
      const std::string& guid,
      ::onc::ONCSource* onc_source) const override;

  void ResetDNSProperties(const std::string& service_path) override;

  bool HasAnyPolicyNetwork(const std::string& userhash) const override;

  const base::Value::Dict* GetGlobalConfigFromPolicy(
      const std::string& userhash) const override;

  const base::Value::Dict* FindPolicyByGuidAndProfile(
      const std::string& guid,
      const std::string& profile_path,
      PolicyType policy_type,
      ::onc::ONCSource* onc_source,
      std::string* userhash) const override;

  bool IsNetworkConfiguredByPolicy(
      const std::string& guid,
      const std::string& profile_path) const override;

  bool CanRemoveNetworkConfig(const std::string& guid,
                              const std::string& profile_path) const override;

  // This method should be called when the policy has been fully applied and is
  // reflected in NetworkStateHandler, so it is safe to notify observers.
  // Notifying observers is the last step of policy application to
  // |service_path|.
  void NotifyPolicyAppliedToNetwork(
      const std::string& service_path) const override;

  void TriggerEphemeralNetworkConfigActions() override;

  void TriggerCellularPolicyApplication(
      const NetworkProfile& profile,
      const base::flat_set<std::string>& new_cellular_policy_guids);
  void OnCellularPoliciesApplied(const NetworkProfile& profile) override;

  PolicyTextMessageSuppressionState GetAllowTextMessages() const override;
  bool AllowApnModification() const override;
  bool AllowCellularSimLock() const override;
  bool AllowCellularHotspot() const override;
  bool AllowOnlyPolicyCellularNetworks() const override;
  bool AllowOnlyPolicyWiFiToConnect() const override;
  bool AllowOnlyPolicyWiFiToConnectIfAvailable() const override;
  bool AllowOnlyPolicyNetworksToAutoconnect() const override;
  bool IsProhibitedFromConfiguringVpn() const override;

  bool RecommendedValuesAreEphemeral() const override;
  bool UserCreatedNetworkConfigurationsAreEphemeral() const override;
  std::vector<std::string> GetBlockedHexSSIDs() const override;

  // NetworkProfileObserver overrides
  void OnProfileAdded(const NetworkProfile& profile) override;
  void OnProfileRemoved(const NetworkProfile& profile) override;

  // PolicyApplicator::ConfigurationHandler overrides
  void CreateConfigurationFromPolicy(const base::Value::Dict& shill_properties,
                                     base::OnceClosure callback) override;

  void UpdateExistingConfigurationWithPropertiesFromPolicy(
      const base::Value::Dict& existing_properties,
      const base::Value::Dict& new_properties,
      base::OnceClosure callback) override;

  void OnEnterpriseMonitoredWebPoliciesApplied() const override;

  void OnPoliciesApplied(
      const NetworkProfile& profile,
      const base::flat_set<std::string>& new_cellular_policy_guids) override;

  void Shutdown() override;

 private:
  friend class AutoConnectHandlerTest;
  friend class ClientCertResolverTest;
  friend class ESimPolicyLoginMetricsLoggerTest;
  friend class ManagedNetworkConfigurationHandler;
  friend class ManagedNetworkConfigurationHandlerTest;
  friend class ManagedNetworkConfigurationHandlerMockTest;
  friend class NetworkConnectionHandlerImplTest;
  friend class NetworkHandler;
  friend class ProhibitedTechnologiesHandlerTest;

  // This structure holds information about the status of ONC network policy
  // application for a shill profile.
  // ManagedNetworkConfigurationHandler maintains a map shill profile ->
  // PolicyApplicationInfo.
  struct PolicyApplicationInfo {
    PolicyApplicationInfo();
    ~PolicyApplicationInfo();

    // Moveable type
    PolicyApplicationInfo(const PolicyApplicationInfo& other) = delete;
    PolicyApplicationInfo& operator=(const PolicyApplicationInfo& other) =
        delete;
    PolicyApplicationInfo(PolicyApplicationInfo&& other);
    PolicyApplicationInfo& operator=(PolicyApplicationInfo&& other);

    bool IsRunningOrRequired() const {
      return application_required || running_policy_applicator;
    }

    // Holds the set of ONC NetworkConfiguration GUIDs which have been modified
    // since network policy has been last applied.
    base::flat_set<std::string> modified_policy_guids;
    // Additional PolicyApplicator options.
    PolicyApplicator::Options options;
    // If true, network policy application needs to happen for this shill
    // profile, i.e. there were network policy changes that have not been
    // applied yet. Note that this can be true even if |modified_policy_guids|
    // is empty, e.g. if an ONC GlobalNetworkConfiguration parameter (which
    // affects all networks in this shill profile) has changed, but the settings
    // of the individual NetworkConfigurations remained the same.
    bool application_required = false;
    // If true, a task has already been scheduled to actually apply network
    // policy for this shill profile.
    bool task_scheduled = false;
    // If present, network policy is currently being applied (which is an
    // asynchronous process). The PolicyApplicator instance is responsible for
    // applying it.
    std::unique_ptr<PolicyApplicator> running_policy_applicator;
  };

  using UserToPoliciesMap =
      base::flat_map<std::string, std::unique_ptr<ProfilePolicies>>;
  using UserToPolicyApplicationInfo =
      base::flat_map<std::string, PolicyApplicationInfo>;

  // The type of properties to send after a Get{Managed}Properties call.
  enum class PropertiesType {
    kUnmanaged,
    kManaged,
  };

  ManagedNetworkConfigurationHandlerImpl();

  // Handlers may be null in tests so long as they do not execute any paths
  // that require the handlers.
  void Init(CellularPolicyHandler* cellular_policy_handler,
            ManagedCellularPrefHandler* managed_cellular_pref_handler,
            NetworkStateHandler* network_state_handler,
            NetworkProfileHandler* network_profile_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            NetworkDeviceHandler* network_device_handler,
            ProhibitedTechnologiesHandler* prohibited_technologies_handler,
            HotspotController* hotspot_controller);

  // Returns the ProfilePolicies for the given |userhash|, or the device
  // policies if |userhash| is empty. Creates the ProfilePolicies entry if it
  // does not exist yet.
  ProfilePolicies* GetOrCreatePoliciesForUser(const std::string& userhash);
  // Returns the ProfilePolicies for the given |userhash|, or the device
  // policies if |userhash| is empty.
  const ProfilePolicies* GetPoliciesForUser(const std::string& userhash) const;
  // Returns the ProfilePolicies for the given network |profile|. These could be
  // either user or device policies.
  const ProfilePolicies* GetPoliciesForProfile(
      const NetworkProfile& profile) const;

  // Called when a policy identified by |guid| has been applied to the network
  // identified by |service_path|. Notifies observers and calls |callback|.
  void OnPolicyAppliedToNetwork(base::OnceClosure callback,
                                const std::string& service_path,
                                const std::string& guid) const;

  // Helper method to append associated Device properties to |properties|.
  void GetDeviceStateProperties(const std::string& service_path,
                                base::Value::Dict* properties);

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
                             std::optional<base::Value::Dict> shill_properties);

  // Implemented as a callback for GetProperties, fetches Type,
  // IPAddressConfig, StaticIPConfig, and changes the
  // NameServersConfigType ONC property to be automatically set by DHCP and
  // applies it to a specific network device.
  void ResetDNSPropertiesCallback(
      const std::string& service_path,
      std::optional<base::Value::Dict> network_properties,
      std::optional<std::string> error);

  void OnGetDeviceProperties(
      PropertiesType properties_type,
      const std::string& userhash,
      const std::string& service_path,
      network_handler::PropertiesCallback callback,
      std::optional<base::Value::Dict> network_properties,
      const std::string& device_path,
      std::optional<base::Value::Dict> device_properties);

  void SendProperties(PropertiesType properties_type,
                      const std::string& userhash,
                      const std::string& service_path,
                      network_handler::PropertiesCallback callback,
                      std::optional<base::Value::Dict> shill_properties);

  // Called from SetProperties, calls NCH::SetShillProperties.
  void SetShillProperties(const std::string& service_path,
                          base::Value::Dict shill_dictionary,
                          base::OnceClosure callback,
                          network_handler::ErrorCallback error_callback);

  // Sets the active proxy values in managed network configurations depending on
  // the source of the configuration. Proxy enforced by user policy
  // (provided by kProxy preference) should have precedence over configurations
  // set by ONC policy.
  void SetManagedActiveProxyValues(const std::string& guid,
                                   base::Value::Dict* dictionary);

  // Applies policies for |userhash|.
  // |modified_policies| contains the GUIDs of the network configurations that
  // changed since the last policy application. |can_affect_other_networks|
  // should be true if the operation that led to this call may have changed
  // effective settings of network configurations that are not in the
  // |modified_policies| list, e.g. because a global network setting has
  // changed.
  void ApplyOrQueuePolicies(const std::string& userhash,
                            base::flat_set<std::string> modified_policies,
                            bool can_affect_other_networks,
                            PolicyApplicator::Options options);

  // Called in SetPolicy, sets shill DisconnectWiFiOnEthernet Manager property
  // base on value of DisconnectWiFiOnEthernet GlobalNetworkConfiguration.
  void ApplyDisconnectWiFiOnEthernetPolicy();

  void SchedulePolicyApplication(const std::string& userhash);
  void StartPolicyApplication(const std::string& userhash);

  // Based on the admin policy to allow usage of custom APNs, this method sets
  // or clears custom apn list in shill properties.
  void ModifyCustomAPNs();

  void set_ui_proxy_config_service(
      UIProxyConfigService* ui_proxy_config_service);

  void set_user_prefs(PrefService* user_prefs);

  NetworkMetadataStore* GetNetworkMetadataStore();

  void set_network_metadata_store_for_testing(
      NetworkMetadataStore* network_metadata_store_for_testing) {
    network_metadata_store_for_testing_ = network_metadata_store_for_testing;
  }

  // Returns the device policy GlobalNetworkConfiguration boolean value under
  // `key` or `std::nullopt` if such a value doesn't exist or is not of type
  // BOOLEAN.
  std::optional<bool> FindGlobalPolicyBool(std::string_view key) const;
  // Returns the device policy GlobalNetworkConfiguration List value under
  // `key` or `nullptr` if such a value doesn't exist or is not of type LIST.
  const base::Value::List* FindGlobalPolicyList(std::string_view key) const;
  // Returns the device policy GlobalNetworkConfiguration string value under
  // `key` or `nullptr` if such a value doesn't exist or is not of type STRING.
  const std::string* FindGlobalPolicyString(std::string_view key) const;

  // If present, the empty string maps to the device policy.
  UserToPoliciesMap policies_by_user_;

  // Local references to the associated handler instances.
  raw_ptr<CellularPolicyHandler, DanglingUntriaged> cellular_policy_handler_ =
      nullptr;
  raw_ptr<ManagedCellularPrefHandler, DanglingUntriaged>
      managed_cellular_pref_handler_ = nullptr;
  raw_ptr<NetworkStateHandler, DanglingUntriaged> network_state_handler_ =
      nullptr;
  raw_ptr<NetworkProfileHandler> network_profile_handler_ = nullptr;
  raw_ptr<NetworkConfigurationHandler, DanglingUntriaged>
      network_configuration_handler_ = nullptr;
  raw_ptr<NetworkDeviceHandler, DanglingUntriaged> network_device_handler_ =
      nullptr;
  raw_ptr<ProhibitedTechnologiesHandler, DanglingUntriaged>
      prohibited_technologies_handler_ = nullptr;
  raw_ptr<UIProxyConfigService, DanglingUntriaged> ui_proxy_config_service_ =
      nullptr;
  raw_ptr<HotspotController, DanglingUntriaged> hotspot_controller_ = nullptr;
  raw_ptr<NetworkMetadataStore> network_metadata_store_for_testing_ = nullptr;

  // Initialized to null and set once SetUserPrefs() is called.
  raw_ptr<PrefService> user_prefs_ = nullptr;

  UserToPolicyApplicationInfo policy_application_info_map_;

  base::ObserverList<NetworkPolicyObserver, true>::Unchecked observers_;

  bool user_policy_applied_ = false;
  bool device_policy_applied_ = false;
  // Ensure that Shutdown() gets called exactly once.
  bool did_shutdown_ = false;

  // For Shill client callbacks
  base::WeakPtrFactory<ManagedNetworkConfigurationHandlerImpl>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
