// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_LIST_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_LIST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_export.h"

class PrefValueMap;

namespace policy {

class ConfigurationPolicyHandler;
struct PolicyDetails;
class PolicyErrorMap;
struct PolicyHandlerParameters;
class Schema;

// Converts policies to their corresponding preferences by applying a list of
// ConfigurationPolicyHandler objects. This includes error checking and cleaning
// up policy values for display.
class POLICY_EXPORT ConfigurationPolicyHandlerList {
 public:
  typedef base::RepeatingCallback<void(PolicyHandlerParameters*)>
      PopulatePolicyHandlerParametersCallback;

  explicit ConfigurationPolicyHandlerList(
      const PopulatePolicyHandlerParametersCallback& parameters_callback,
      const GetChromePolicyDetailsCallback& details_callback,
      bool are_future_policies_allowed_by_default);
  ConfigurationPolicyHandlerList(const ConfigurationPolicyHandlerList&) =
      delete;
  ConfigurationPolicyHandlerList& operator=(
      const ConfigurationPolicyHandlerList&) = delete;
  ~ConfigurationPolicyHandlerList();

  // Adds a policy handler to the list.
  void AddHandler(std::unique_ptr<ConfigurationPolicyHandler> handler);

  // Translates |policies| to their corresponding preferences in |prefs|. Any
  // errors found while processing the policies are stored in |errors|.
  // All deprecated policies will be stored into |deprecated_policies|.
  // All non-applying unreleased policies will be stored in
  // |future_policies_blocked|. |prefs|, |deprecated_policies|,
  // |future_policies_blocked| or |errors| can be nullptr, and won't be
  // filled in that case.
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs,
                           PolicyErrorMap* errors,
                           PoliciesSet* deprecated_policies,
                           PoliciesSet* future_policies_blocked) const;

  // Converts sensitive policy values to others more appropriate for displaying.
  void PrepareForDisplaying(PolicyMap* policies) const;

 private:
  // Returns true if the policy |entry| shouldn't be passed to the |handlers_|.
  // On Stable and Beta channel, future policies that are not in the
  // |future_policies_allowed| will be filtered out and put into the
  // |future_policies_blocked|.
  bool IsPolicySupported(
      const base::flat_set<std::string>& future_policies_allowed,
      PoliciesSet* future_policies_blocked,
      PolicyMap::const_reference entry) const;

  bool IsBlockedPlatformDevicePolicy(const PolicyDetails& policy_details,
                                     PolicyMap::const_reference entry) const;

  bool IsBlockedFuturePolicy(
      const base::flat_set<std::string>& future_policies_allowed,
      const PolicyDetails& policy_details,
      PolicyMap::const_reference entry) const;

  std::vector<std::unique_ptr<ConfigurationPolicyHandler>> handlers_;
  const PopulatePolicyHandlerParametersCallback parameters_callback_;
  const GetChromePolicyDetailsCallback details_callback_;

  bool are_future_policies_allowed_by_default_ = false;
};

// Callback with signature of BuildHandlerList(), to be used in constructor of
// BrowserPolicyConnector.
using HandlerListFactory =
    base::RepeatingCallback<std::unique_ptr<ConfigurationPolicyHandlerList>(
        const Schema&)>;

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_LIST_H_
