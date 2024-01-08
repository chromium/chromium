// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PROFILE_POLICIES_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PROFILE_POLICIES_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/network/client_cert_util.h"

namespace ash {

// Stores network policies for a shill profile.
// Understands some ONC (OpenNetworkConfiguration) concepts such as
// NetworkConfiguration and ONC variable expansion (
// https://chromium.googlesource.com/chromium/src/+/main/components/onc/docs/onc_spec.md#String-Expansions
// ).
// Variable expansions can be set on the ProfilePolicies level using
// ProfilePolicies::SetProfileWideExpansions where they apply to all network
// configurations within that ProfilePolicies.
// On a single network policy level (keyed by the network policy's GUID),
// ProfilePolicies::SetResolvedClientCertificate can be used to set the resolved
// certificate including variable expansions extracted from that certificate.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ProfilePolicies {
 public:
  enum class ChangeEffect { kNoChange, kEffectivePolicyChanged };

  // Stores policies for a network.
  class NetworkPolicy {
   public:
    NetworkPolicy(const ProfilePolicies* parent, base::Value::Dict onc_policy);
    ~NetworkPolicy();

    NetworkPolicy(const NetworkPolicy& other) = delete;
    NetworkPolicy& operator=(const NetworkPolicy& other) = delete;

    NetworkPolicy(NetworkPolicy&& other);
    NetworkPolicy& operator=(NetworkPolicy&& other);

    // Replaces the current policy with |new_policy|, which should be an ONC
    // NetworkConfiguration.
    // Returns an indication about whether the effective policy has changed as a
    // result of this call.
    ChangeEffect UpdateFrom(const base::Value::Dict& new_onc_policy);

    // Sets the resolved client certificate for this network.
    // Returns an indication about whether the effective policy has changed as a
    // result of this call.
    ChangeEffect SetResolvedClientCertificate(
        client_cert::ResolvedCert resolved_cert);

    // Re-applies the profile-wide expansions from |parent|. Should be called by
    // |parent| whenever profile-wide expansions have changed.
    // Returns an indication about whether the effective policy has changed as a
    // result of this call.
    ChangeEffect OnProfileWideExpansionsChanged();

    // Returns the original ONC policy without runtime values.
    const base::Value::Dict& GetOriginalPolicy() const;

    // Returns the effective ONC policy with runtime values set.
    const base::Value::Dict& GetPolicyWithRuntimeValues() const;

   private:
    // Applies the runtime values.
    ChangeEffect ReapplyRuntimeValues();

    raw_ptr<const ProfilePolicies> parent_;

    client_cert::ResolvedCert resolved_cert_ =
        client_cert::ResolvedCert::NotKnownYet();

    base::Value::Dict original_policy_;

    // The ONC NetworkConfiguration with runtime values set.  If this is absent,
    // it means that setting runtime values didn't change anything compared to
    // |original_onc_policy_|.
    std::optional<base::Value::Dict> policy_with_runtime_values_;
  };

  // Used to check whether an ONC NetworkConfiguration passed in
  // |onc_network_configuration| has the same identifying properties as the
  // shill properties dictionary |shill_properties|.
  using ShillPropertiesMatcher = base::RepeatingCallback<bool(
      const base::Value::Dict& onc_network_configuration,
      const base::Value::Dict& shill_properties)>;

  using RuntimeValuesSetter = base::RepeatingCallback<base::Value::Dict(
      const base::Value::Dict& onc_network_configuration,
      const base::flat_map<std::string, std::string>& profile_wide_expansions,
      const client_cert::ResolvedCert& resolved_cert)>;

  ProfilePolicies();
  ~ProfilePolicies();

  ProfilePolicies(const ProfilePolicies& other) = delete;
  ProfilePolicies& operator=(const ProfilePolicies& other) = delete;

  // Applies the ONC NetworkConfiguration entries from |network_configs_onc|.
  // Returns the set of policy GUIDs that have effectively changed as a result
  // of this operation.
  base::flat_set<std::string> ApplyOncNetworkConfigurationList(
      const base::Value::List& network_configs_onc);

  // Overwrites the ONC GlobalNetworkConfiguration dictionary with
  // |global_network_config|.
  void SetGlobalNetworkConfig(const base::Value::Dict& global_network_config);

  // Sets the ONC variable expansions which should apply to all
  // NetworkConfigurations within this ProfilePolicies instance.
  // Returns the set of policy GUIDs which have effectively changed due to this.
  base::flat_set<std::string> SetProfileWideExpansions(
      base::flat_map<std::string, std::string> expansions);

  // Sets the resolved client certificate for the ONC NetworkConfiguration
  // specified by |guid|.
  // Returns true if the policy for |guid| has effectively changed, false
  // otherwise.
  // If |guid| does not refer to a NetworkConfiguration from policy, the
  // |resolved_cert| will be lost and false is returned.
  bool SetResolvedClientCertificate(const std::string& guid,
                                    client_cert::ResolvedCert resolved_cert);

  // Returns the policy for |guid| or nullptr if no such policy exists.
  // If the policy value contained ONC variable expansions, they will be
  // expanded in the returned value. The returned pointer remains valid as long
  // as this instance is valid and is not modified (e.g. by calls to
  // SetProfileWideExpansions).
  const base::Value::Dict* GetPolicyByGuid(const std::string& guid) const;

  // Returns the policy for |guid| without runtime values set (i.e. the
  // variable placeholders such as ${LOGIN_EMAIL} will still be present), or
  // nullptr if no such policy exists. The returned pointer remains valid as
  // long as this instance is valid and is not modified (e.g. by calls to
  // SetProfileWideExpansions).
  const base::Value::Dict* GetOriginalPolicyByGuid(
      const std::string& guid) const;

  // Returns the GlobalNetworkConfiguration ONC Dictionary.
  // This will never return nullptr (if no GlobalNetworkConfiguration has been
  // set, it will return a pointer to an empty dictionary).
  // The returned pointer remains valid as long as this instance is valid and is
  // not modified (e.g. by calls to SetGlobalNetworkConfig).
  const base::Value::Dict* GetGlobalNetworkConfig() const {
    return &global_network_config_;
  }

  // Returns true if ProfilePolicies contains a policy value which would apply
  // to the shill property dictionary |shill_properties|.
  bool HasPolicyMatchingShillProperties(
      const base::Value::Dict& shill_properties) const;

  // Returns the map of network policy GUID to ONC NetworkConfiguration.
  // The returned policy values will have ONC variables expanded, if they
  // contained any.
  // This clones all values in the map.
  base::flat_map<std::string, base::Value::Dict> GetGuidToPolicyMap() const;

  // Returns the set of all network policy GUIDs.
  base::flat_set<std::string> GetAllPolicyGuids() const;

  // Sets the matcher which will perform matching of ONC NetworkConfiguration
  // values against shill property dictionaries in
  // HasPolicyMatchingShillProperties. This is useful so the unit test of this
  // class does not have to depend on the actual matching logic (which is unit
  // tested elsewhere).
  void SetShillPropertiesMatcherForTesting(
      const ShillPropertiesMatcher& shill_properties_matcher);

  // Sets the function which will be executed to set run-time values in the ONC
  // NetworkConfiguration.
  // This is useful so the unit test of this class does not have to depend on
  // the actual ONC logic (which is unit tested elsewhere).
  void SetRuntimeValuesSetterForTesting(
      const RuntimeValuesSetter& runtime_values_setter);

 private:
  NetworkPolicy* FindPolicy(const std::string& guid);
  const NetworkPolicy* FindPolicy(const std::string& guid) const;

  ShillPropertiesMatcher shill_properties_matcher_;

  // Sets values computed at runtime into an ONC NetworkConfiguration
  // dictionary (currently variable expansions and a resolved client
  // certificate).
  RuntimeValuesSetter runtime_values_setter_;

  base::flat_map<std::string, NetworkPolicy> guid_to_policy_;
  base::Value::Dict global_network_config_;

  base::flat_map<std::string, std::string> profile_wide_expansions_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PROFILE_POLICIES_H_
