// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler_list.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

// Don't show errors for policies starting with that prefix.
const char kPolicyCommentPrefix[] = "_comment";

}  // namespace

ConfigurationPolicyHandlerList::ConfigurationPolicyHandlerList(
    const PopulatePolicyHandlerParametersCallback& parameters_callback,
    const GetChromePolicyDetailsCallback& details_callback,
    bool are_future_policies_allowed_by_default)
    : parameters_callback_(parameters_callback),
      details_callback_(details_callback),
      are_future_policies_allowed_by_default_(
          are_future_policies_allowed_by_default) {}

ConfigurationPolicyHandlerList::~ConfigurationPolicyHandlerList() {
}

void ConfigurationPolicyHandlerList::AddHandler(
    std::unique_ptr<ConfigurationPolicyHandler> handler) {
  handlers_.push_back(std::move(handler));
}

void ConfigurationPolicyHandlerList::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs,
    PolicyErrorMap* errors,
    PoliciesSet* deprecated_policies,
    PoliciesSet* future_policies_blocked) const {
  if (deprecated_policies)
    deprecated_policies->clear();
  if (future_policies_blocked) {
    future_policies_blocked->clear();
  }
  // This function is used both to apply the policy settings, and to check them
  // and list errors. As such it must get all the errors even if it isn't
  // applying the policies.
  base::flat_set<std::string> future_policies_allowed =
      are_future_policies_allowed_by_default_
          ? base::flat_set<std::string>()
          : ValueToStringSet(policies.GetValue(key::kEnableExperimentalPolicies,
                                               base::Value::Type::LIST));
  PolicyMap filtered_policies = policies.CloneIf(
      base::BindRepeating(&ConfigurationPolicyHandlerList::IsPolicySupported,
                          base::Unretained(this), future_policies_allowed,
                          future_policies_blocked));

  PolicyErrorMap scoped_errors;
  if (!errors)
    errors = &scoped_errors;

  PolicyHandlerParameters parameters;
  if (parameters_callback_)
    parameters_callback_.Run(&parameters);

  for (const auto& handler : handlers_) {
    if (handler->CheckPolicySettings(filtered_policies, errors) && prefs) {
      handler->ApplyPolicySettingsWithParameters(filtered_policies, parameters,
                                                 prefs);
    }
  }

  if (details_callback_ && deprecated_policies) {
    for (const auto& key_value : filtered_policies) {
      const PolicyDetails* details = details_callback_.Run(key_value.first);
      if (details && details->is_deprecated)
        deprecated_policies->insert(key_value.first);
    }
  }
}

void ConfigurationPolicyHandlerList::PrepareForDisplaying(
    PolicyMap* policies) const {
  for (const auto& handler : handlers_)
    handler->PrepareForDisplaying(policies);
}

bool ConfigurationPolicyHandlerList::IsPolicySupported(
    const base::flat_set<std::string>& future_policies_allowed,
    PoliciesSet* future_policies_blocked,
    PolicyMap::const_reference entry) const {
  // Callback might be missing in tests.
  if (!details_callback_) {
    CHECK_IS_TEST();
    return true;
  }

  const PolicyDetails* policy_details = details_callback_.Run(entry.first);
  if (!policy_details) {
    const std::string prefix(kPolicyCommentPrefix);
    if (entry.first.compare(0, prefix.length(), prefix) != 0) {
      DVLOG_POLICY(1, POLICY_PROCESSING) << "Unknown policy: " << entry.first;
    }
    return true;
  }

  if (IsBlockedFuturePolicy(future_policies_allowed, *policy_details, entry)) {
    if (future_policies_blocked) {
      future_policies_blocked->insert(entry.first);
    }
    return false;
  }

  return !IsBlockedPlatformDevicePolicy(*policy_details, entry);
}

bool ConfigurationPolicyHandlerList::IsBlockedPlatformDevicePolicy(
    const PolicyDetails& policy_details,
    PolicyMap::const_reference entry) const {
  if (entry.second.source == POLICY_SOURCE_PLATFORM &&
      policy_details.scope == kDevice) {
    // Device Policy is only implemented as Cloud Policy (not Platform Policy).
    LOG_POLICY(WARNING, POLICY_PROCESSING)
        << "Ignoring device platform policy: " << entry.first;
    return true;
  }
  return false;
}

bool ConfigurationPolicyHandlerList::IsBlockedFuturePolicy(
    const base::flat_set<std::string>& future_policies_allowed,
    const PolicyDetails& policy_details,
    PolicyMap::const_reference entry) const {
  return !are_future_policies_allowed_by_default_ && policy_details.is_future &&
         !future_policies_allowed.contains(entry.first);
}

}  // namespace policy
