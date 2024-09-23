// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/idle_timeout_policy_handler.h"

#include <string>

#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/browsing_data/core/browsing_data_policies_utils.h"
#include "components/enterprise/idle/action_type.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_idle {

namespace {

// If `other_policy_name` is unset, adds an error to `errors` and returns false.
bool CheckOtherPolicySet(const policy::PolicyMap& policies,
                         const std::string& this_policy_name,
                         const std::string& other_policy_name,
                         policy::PolicyErrorMap* errors) {
  if (policies.GetValueUnsafe(other_policy_name)) {
    return true;
  }

  errors->AddError(this_policy_name, IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                   other_policy_name);
  return false;
}

bool CheckPolicyScopeSupported(const policy::PolicyMap& policies,
                               const std::string& policy_name,
                               policy::PolicyErrorMap* errors) {
// The policies will not be supported as user policies on iOS until we clear
// data on sign out for managed users which requires new UI.
#if BUILDFLAG(IS_IOS)
  bool is_user_policy =
      policies.Get(policy_name)->scope == policy::POLICY_SCOPE_USER;
  if (is_user_policy) {
    errors->AddError(policy_name,
                     IDS_POLICY_NOT_SUPPORTED_AS_USER_POLICY_ON_IOS);
  }
  return !is_user_policy;
#else
  // Return true on all other platforms.
  return true;
#endif  // BUILDFLAG(IS_IOS)
}

}  // namespace

IdleTimeoutPolicyHandler::IdleTimeoutPolicyHandler()
    : policy::IntRangePolicyHandler(policy::key::kIdleTimeout,
                                    prefs::kIdleTimeout,
                                    1,
                                    INT_MAX,
                                    true) {}

IdleTimeoutPolicyHandler::~IdleTimeoutPolicyHandler() = default;

void IdleTimeoutPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  DCHECK(value);

  // Apply a minimum of 1.
  base::TimeDelta time_delta = base::Minutes(std::max(value->GetInt(), 1));
  prefs->SetValue(prefs::kIdleTimeout, base::TimeDeltaToValue(time_delta));
}

bool IdleTimeoutPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy_name())) {
    return false;
  }

  // Check that it's an integer, and that it's >= 1.
  if (!policy::IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // If IdleTimeoutActions is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy_name(),
                           policy::key::kIdleTimeoutActions, errors)) {
    return false;
  }

  if (!CheckPolicyScopeSupported(policies, policy_name(), errors)) {
    return false;
  }

  return true;
}

IdleTimeoutActionsPolicyHandler::IdleTimeoutActionsPolicyHandler(
    policy::Schema schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kIdleTimeoutActions,
          schema.GetKnownProperty(policy::key::kIdleTimeoutActions),
          policy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

IdleTimeoutActionsPolicyHandler::~IdleTimeoutActionsPolicyHandler() = default;

void IdleTimeoutActionsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  DCHECK(policy_value);

  // Convert strings to integers (from the ActionType enum).
  base::Value::List converted_actions;
  for (const base::Value& action : policy_value->GetList()) {
    if (!action.is_string()) {
      continue;
    }
    if (std::optional<ActionType> action_type =
            NameToActionType(action.GetString())) {
      converted_actions.Append(static_cast<int>(action_type.value()));
    }
  }
  prefs->SetValue(prefs::kIdleTimeoutActions,
                  base::Value(std::move(converted_actions)));

  std::string log_message = browsing_data::DisableSyncTypes(
      forced_disabled_sync_types_, prefs, policy_name());
  if (!log_message.empty()) {
    LOG_POLICY(INFO, POLICY_PROCESSING) << log_message;
  }

#if BUILDFLAG(IS_IOS)
  // Set the `kIdleTimeoutPolicyAppliesToUserOnly`pref if the policy is set as a
  // user policy. This will determine whether data should be cleared for
  // `TimePeriod::ALL_TIME` or only for the time the user was signed in.
  bool user_policy =
      policies.Get(policy_name())->scope == policy::POLICY_SCOPE_USER;
  prefs->SetBoolean(prefs::kIdleTimeoutPolicyAppliesToUserOnly, user_policy);
#endif  // BUILDFLAG(IS_IOS)
}

bool IdleTimeoutActionsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy_name())) {
    return false;
  }

  // Check that it's a list of strings, and that they're supported enum values.
  // Unsupported enum values are dropped, with a warning on chrome://policy.
  if (!policy::SchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                  errors)) {
    return false;
  }

  // If IdleTimeout is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy_name(), policy::key::kIdleTimeout,
                           errors)) {
    return false;
  }

  if (!CheckPolicyScopeSupported(policies, policy_name(), errors)) {
    return false;
  }

#if !BUILDFLAG(IS_ANDROID)
  const base::Value* sync_disabled =
      policies.GetValue(policy::key::kSyncDisabled, base::Value::Type::BOOLEAN);
  if (sync_disabled && sync_disabled->GetBool()) {
    return true;
  }
#endif  //! BUILDFLAG(IS_ANDROID)

// BrowserSignin policy is not available on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  const auto* browser_signin_disabled = policies.GetValue(
      policy::key::kBrowserSignin, base::Value::Type::INTEGER);
  if (browser_signin_disabled && browser_signin_disabled->GetInt() == 0) {
    return true;
  }
#endif

  // Automatically disable sync for the required data types.
  const base::Value* value =
      policies.GetValue(this->policy_name(), base::Value::Type::LIST);
  DCHECK(value);
  base::Value::List clear_data_actions;
  for (const base::Value& action : value->GetList()) {
    if (!action.is_string()) {
      continue;
    }
    std::string clear_data_action =
        GetActionBrowsingDataTypeName(action.GetString());
    if (!clear_data_action.empty()) {
      clear_data_actions.Append(clear_data_action);
    }
  }
  forced_disabled_sync_types_ = browsing_data::GetSyncTypesForClearBrowsingData(
      base::Value(std::move(clear_data_actions)));

  return true;
}

// TODO(esalma): Move this logic to `ApplyPolicySettings()` after fixing
// crbug.com/1435069.
void IdleTimeoutActionsPolicyHandler::PrepareForDisplaying(
    policy::PolicyMap* policies) const {
  policy::PolicyMap::Entry* entry = policies->GetMutable(policy_name());
  if (!entry || forced_disabled_sync_types_.empty()) {
    return;
  }
  // `PolicyConversionsClient::GetPolicyValue()` doesn't support
  // MessageType::kInfo in the PolicyErrorMap, so add the message to the policy
  // when it is prepared to be displayed on chrome://policy.
  entry->AddMessage(policy::PolicyMap::MessageType::kInfo,
                    IDS_POLICY_BROWSING_DATA_DEPENDENCY_APPLIED_INFO,
                    {base::UTF8ToUTF16(UserSelectableTypeSetToString(
                        forced_disabled_sync_types_))});
}

}  // namespace enterprise_idle
