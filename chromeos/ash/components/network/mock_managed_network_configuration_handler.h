// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_

#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockManagedNetworkConfigurationHandler
    : public ManagedNetworkConfigurationHandler {
 public:
  MockManagedNetworkConfigurationHandler();

  MockManagedNetworkConfigurationHandler(
      const MockManagedNetworkConfigurationHandler&) = delete;
  MockManagedNetworkConfigurationHandler& operator=(
      const MockManagedNetworkConfigurationHandler&) = delete;

  ~MockManagedNetworkConfigurationHandler() override;

  // ManagedNetworkConfigurationHandler overrides
  MOCK_METHOD1(AddObserver, void(NetworkPolicyObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(NetworkPolicyObserver* observer));
  MOCK_CONST_METHOD1(HasObserver, bool(NetworkPolicyObserver* observer));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD3(GetProperties,
               void(const std::string& userhash,
                    const std::string& service_path,
                    network_handler::PropertiesCallback callback));
  MOCK_METHOD3(GetManagedProperties,
               void(const std::string& userhash,
                    const std::string& service_path,
                    network_handler::PropertiesCallback callback));
  MOCK_METHOD4(SetProperties,
               void(const std::string& service_path,
                    const base::Value::Dict& user_settings,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));
  MOCK_METHOD4(ClearShillProperties,
               void(const std::string& service_path,
                    const std::vector<std::string>& names,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));
  MOCK_CONST_METHOD4(CreateConfiguration,
                     void(const std::string& userhash,
                          const base::Value::Dict& properties,
                          network_handler::ServiceResultCallback callback,
                          network_handler::ErrorCallback error_callback));
  MOCK_CONST_METHOD2(ConfigurePolicyNetwork,
                     void(const base::Value::Dict& shill_properties,
                          base::OnceClosure callback));
  MOCK_CONST_METHOD3(RemoveConfiguration,
                     void(const std::string& service_path,
                          base::OnceClosure callback,
                          network_handler::ErrorCallback error_callback));
  MOCK_CONST_METHOD3(RemoveConfigurationFromCurrentProfile,
                     void(const std::string& service_path,
                          base::OnceClosure callback,
                          network_handler::ErrorCallback error_callback));
  MOCK_METHOD4(SetPolicy,
               void(::onc::ONCSource onc_source,
                    const std::string& userhash,
                    const base::Value::List& network_configs_onc,
                    const base::Value::Dict& global_network_config));
  MOCK_CONST_METHOD0(IsAnyPolicyApplicationRunning, bool());
  MOCK_METHOD2(SetProfileWideVariableExpansions,
               void(const std::string& userhash,
                    base::flat_map<std::string, std::string> expansions));
  MOCK_METHOD3(SetResolvedClientCertificate,
               bool(const std::string& userhash,
                    const std::string& guid,
                    client_cert::ResolvedCert resolved_cert));
  MOCK_CONST_METHOD3(FindPolicyByGUID,
                     const base::Value::Dict*(const std::string userhash,
                                              const std::string& guid,
                                              ::onc::ONCSource* onc_source));
  MOCK_METHOD1(ResetDNSProperties, void(const std::string& service_path));
  MOCK_CONST_METHOD1(HasAnyPolicyNetwork, bool(const std::string& userhash));
  MOCK_CONST_METHOD1(GetGlobalConfigFromPolicy,
                     const base::Value::Dict*(const std::string& userhash));
  MOCK_CONST_METHOD5(FindPolicyByGuidAndProfile,
                     const base::Value::Dict*(const std::string& guid,
                                              const std::string& profile_path,
                                              PolicyType policy_type,
                                              ::onc::ONCSource* out_onc_source,
                                              std::string* out_userhash));
  MOCK_CONST_METHOD2(IsNetworkConfiguredByPolicy,
                     bool(const std::string& guid,
                          const std::string& profile_path));
  MOCK_CONST_METHOD2(CanRemoveNetworkConfig,
                     bool(const std::string& guid,
                          const std::string& profile_path));
  MOCK_CONST_METHOD1(NotifyPolicyAppliedToNetwork,
                     void(const std::string& service_path));
  MOCK_METHOD0(TriggerEphemeralNetworkConfigActions, void());
  MOCK_METHOD1(OnCellularPoliciesApplied, void(const NetworkProfile& profile));
  MOCK_CONST_METHOD0(OnEnterpriseMonitoredWebPoliciesApplied, void());
  MOCK_CONST_METHOD0(AllowApnModification, bool());
  MOCK_CONST_METHOD0(AllowCellularSimLock, bool());
  MOCK_CONST_METHOD0(AllowCellularHotspot, bool());
  MOCK_CONST_METHOD0(AllowOnlyPolicyCellularNetworks, bool());
  MOCK_CONST_METHOD0(AllowOnlyPolicyWiFiToConnect, bool());
  MOCK_CONST_METHOD0(AllowOnlyPolicyWiFiToConnectIfAvailable, bool());
  MOCK_CONST_METHOD0(AllowOnlyPolicyNetworksToAutoconnect, bool());
  MOCK_CONST_METHOD0(IsProhibitedFromConfiguringVpn, bool());
  MOCK_CONST_METHOD0(RecommendedValuesAreEphemeral, bool());
  MOCK_CONST_METHOD0(UserCreatedNetworkConfigurationsAreEphemeral, bool());
  MOCK_CONST_METHOD0(GetAllowTextMessages, PolicyTextMessageSuppressionState());
  MOCK_CONST_METHOD0(GetBlockedHexSSIDs, std::vector<std::string>());
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
