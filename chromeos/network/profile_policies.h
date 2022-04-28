// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PROFILE_POLICIES_H_
#define CHROMEOS_NETWORK_PROFILE_POLICIES_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// Stores network policies for a shill profile.
// Understands some ONC (OpenNetworkConfiguration) concepts such as
// NetworkConfiguration.
// TODO(b/209084821): Add support for variable replacement.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ProfilePolicies {
 public:
  // Stores policies for a network.
  class NetworkPolicy {
   public:
    NetworkPolicy(base::Value onc_policy);
    ~NetworkPolicy();

    NetworkPolicy(const NetworkPolicy& other) = delete;
    NetworkPolicy& operator=(const NetworkPolicy& other) = delete;

    NetworkPolicy(NetworkPolicy&& other);
    NetworkPolicy& operator=(NetworkPolicy&& other);

    // Replaces the current policy with |new_policy|, which should be an ONC
    // NetworkConfiguration.
    // If this NetworkPolicy has changed, returns true. Otherwise returns false.
    bool UpdateFrom(const base::Value& new_policy);

    const base::Value* GetPolicy() const;

   private:
    base::Value onc_policy_;
  };

  // Used to check whether an ONC NetworkConfiguration passed in
  // |onc_network_configuration| has the same identifying properties as the
  // shill properties dictionary |shill_properties|.
  using ShillPropertiesMatcher =
      base::RepeatingCallback<bool(const base::Value& onc_network_configuration,
                                   const base::Value& shill_properties)>;

  ProfilePolicies();
  ~ProfilePolicies();

  ProfilePolicies(const ProfilePolicies& other) = delete;
  ProfilePolicies& operator=(const ProfilePolicies& other) = delete;

  // Applies the ONC NetworkConfiguration entries from |network_configs_onc|,
  // which must be a LIST base::Value.
  // Returns the set of policy GUIDs that have effectively changed as a result
  // of this operation.
  base::flat_set<std::string> ApplyOncNetworkConfigurationList(
      const base::Value& network_configs_onc);

  // Overwirtes the ONC GlobalNetworkConfiguration dictionary with
  // |global_network_config|, which must be a DICTIONARY base::Value.
  void SetGlobalNetworkConfig(const base::Value& global_network_config);

  // Returns the policy for |guid| or nullptr if no such policy exists.
  // The returned pointer remains valid as long as this instance is valid and is
  // not modified (e.g. by calls to ApplyOncNetworkConfigurations).
  const base::Value* GetPolicyByGuid(const std::string& guid) const;

  // Returns the GlobalNetworkConfiguration ONC Dictionary.
  // This will never return nullptr (if no GlobalNetworkConfiguration has been
  // set, it will return a pointer to an empty DICT base::Value). It returns a
  // pointer for convenience at the call sites, which work with base::Value* to
  // be consistent with GetPolicyByGuid return values.
  // The returned pointer remains valid as long as this instance is valid and is
  // not modified (e.g. by calls to SetGlobalNetworkConfig).
  const base::Value* GetGlobalNetworkConfig() const {
    return &global_network_config_;
  }

  // Returns true if ProfilePolicies contains a policy value which would apply
  // to the shill property dictionary |shill_properties|.
  bool HasPolicyMatchingShillProperties(
      const base::Value& shill_properties) const;

  // Returns the map of network policy GUID to ONC NetworkConfiguration.
  // This clones all values in the map.
  base::flat_map<std::string, base::Value> GetGuidToPolicyMap() const;

  // Returns the set of all network policy GUIDs.
  base::flat_set<std::string> GetAllPolicyGuids() const;

  // Sets the matcher which will perform matching of ONC NetworkConfiguration
  // values against shill property dictionaries in
  // HasPolicyMatchingShillProperties. This is useful so the unit test of this
  // class does not have to depend on the actual matching logic (which is unit
  // tested elsewhere).
  void SetShillPropertiesMatcherForTesting(
      const ShillPropertiesMatcher& shill_properties_matcher);

 private:
  NetworkPolicy* FindPolicy(const std::string& guid);
  const NetworkPolicy* FindPolicy(const std::string& guid) const;

  ShillPropertiesMatcher shill_properties_matcher_;

  base::flat_map<std::string, NetworkPolicy> guid_to_policy_;
  base::Value global_network_config_{base::Value::Type::DICTIONARY};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_PROFILE_POLICIES_H_
