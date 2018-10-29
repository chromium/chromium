// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler_list.h"

#include "base/bind.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

// Don't show errors for policies starting with that prefix.
const char kPolicyCommentPrefix[] = "_comment";

}  // namespace

ConfigurationPolicyHandlerList::ConfigurationPolicyHandlerList(
    const PopulatePolicyHandlerParametersCallback& parameters_callback,
    const GetChromePolicyDetailsCallback& details_callback)
    : parameters_callback_(parameters_callback),
      details_callback_(details_callback) {}

ConfigurationPolicyHandlerList::~ConfigurationPolicyHandlerList() {
}

void ConfigurationPolicyHandlerList::AddHandler(
    std::unique_ptr<ConfigurationPolicyHandler> handler) {
  handlers_.push_back(std::move(handler));
}

void ConfigurationPolicyHandlerList::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs,
    PolicyErrorMap* errors) const {
  // This function is used both to apply the policy settings, and to check them
  // and list errors. As such it must get all the errors even if it isn't
  // applying the policies.
  // TODO(aberent): split into two functions.
  std::unique_ptr<PolicyMap> filtered_policies = policies.DeepCopy();
  filtered_policies->EraseMatching(
      base::Bind(&ConfigurationPolicyHandlerList::IsPlatformDevicePolicy,
                 base::Unretained(this)));

  PolicyErrorMap scoped_errors;
  if (!errors)
    errors = &scoped_errors;

  PolicyHandlerParameters parameters;
  parameters_callback_.Run(&parameters);

  for (const auto& handler : handlers_) {
    if (handler->CheckPolicySettings(*filtered_policies, errors) && prefs) {
      handler->ApplyPolicySettingsWithParameters(
          *filtered_policies, parameters, prefs);
    }
  }

  if (details_callback_) {
    for (auto it = filtered_policies->begin(); it != filtered_policies->end();
         ++it) {
      const PolicyDetails* details = details_callback_.Run(it->first);
      if (details && details->is_deprecated)
        errors->AddError(it->first, IDS_POLICY_DEPRECATED);
    }
  }
}

void ConfigurationPolicyHandlerList::PrepareForDisplaying(
    PolicyMap* policies) const {
  for (const auto& handler : handlers_)
    handler->PrepareForDisplaying(policies);
}

bool ConfigurationPolicyHandlerList::IsPlatformDevicePolicy(
    const PolicyMap::const_iterator iter) const {
  // Callback might be missing in tests.
  if (!details_callback_) {
    return false;
  }
  const PolicyDetails* policy_details = details_callback_.Run(iter->first);
  if (!policy_details) {
    const std::string prefix(kPolicyCommentPrefix);
    if (iter->first.compare(0, prefix.length(), prefix) != 0) {
      LOG(ERROR) << "Unknown policy: " << iter->first;
    }
    return false;
  }
  if (iter->second.source == POLICY_SOURCE_PLATFORM &&
      policy_details->is_device_policy) {
    // Device Policy is only implemented as Cloud Policy (not Platform Policy).
    LOG(WARNING) << "Ignoring device platform policy: " << iter->first;
    return true;
  }
  return false;
}

}  // namespace policy
