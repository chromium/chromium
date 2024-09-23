// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/policy_info.h"

#include <string>

#include "base/json/json_writer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/strings/grit/components_strings.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

em::Policy_PolicyLevel GetLevel(const base::Value& policy) {
  switch (
      static_cast<policy::PolicyLevel>(*policy.GetDict().FindInt("level"))) {
    case policy::POLICY_LEVEL_RECOMMENDED:
      return em::Policy_PolicyLevel_LEVEL_RECOMMENDED;
    case policy::POLICY_LEVEL_MANDATORY:
      return em::Policy_PolicyLevel_LEVEL_MANDATORY;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid policy level: " << *policy.GetDict().FindInt("level");
  return em::Policy_PolicyLevel_LEVEL_UNKNOWN;
}

em::Policy_PolicyScope GetScope(const base::Value& policy) {
  switch (
      static_cast<policy::PolicyScope>(*policy.GetDict().FindInt("scope"))) {
    case policy::POLICY_SCOPE_USER:
      return em::Policy_PolicyScope_SCOPE_USER;
    case policy::POLICY_SCOPE_MACHINE:
      return em::Policy_PolicyScope_SCOPE_MACHINE;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid policy scope: " << *policy.GetDict().FindInt("scope");
  return em::Policy_PolicyScope_SCOPE_UNKNOWN;
}

em::Policy_PolicySource GetSource(const base::Value& policy) {
  switch (
      static_cast<policy::PolicySource>(*policy.GetDict().FindInt("source"))) {
    case policy::POLICY_SOURCE_ENTERPRISE_DEFAULT:
      return em::Policy_PolicySource_SOURCE_ENTERPRISE_DEFAULT;
    case policy::POLICY_SOURCE_COMMAND_LINE:
      return em::Policy_PolicySource_SOURCE_COMMAND_LINE;
    case policy::POLICY_SOURCE_CLOUD:
      return em::Policy_PolicySource_SOURCE_CLOUD;
    case policy::POLICY_SOURCE_ACTIVE_DIRECTORY:
      return em::Policy_PolicySource_SOURCE_ACTIVE_DIRECTORY;
    case policy::POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE_DEPRECATED:
      return em::
          Policy_PolicySource_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE_DEPRECATED;
    case policy::POLICY_SOURCE_PLATFORM:
      return em::Policy_PolicySource_SOURCE_PLATFORM;
    case policy::POLICY_SOURCE_PRIORITY_CLOUD_DEPRECATED:
      return em::Policy_PolicySource_SOURCE_PRIORITY_CLOUD_DEPRECATED;
    case policy::POLICY_SOURCE_MERGED:
      return em::Policy_PolicySource_SOURCE_MERGED;
    case policy::POLICY_SOURCE_CLOUD_FROM_ASH:
      return em::Policy_PolicySource_SOURCE_CLOUD_FROM_ASH;
    case policy::POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE:
      return em::
          Policy_PolicySource_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE;
    case policy::POLICY_SOURCE_COUNT:
      NOTREACHED_IN_MIGRATION();
      return em::Policy_PolicySource_SOURCE_UNKNOWN;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid policy source: " << *policy.GetDict().FindInt("source");
  return em::Policy_PolicySource_SOURCE_UNKNOWN;
}

void UpdateConflictedPolicy(em::Policy* policy_info,
                            const base::Value& policy) {
  policy_info->set_level(GetLevel(policy));
  policy_info->set_scope(GetScope(policy));
  policy_info->set_source(GetSource(policy));
}

void UpdateConflictedPolicies(const std::string conflict_key,
                              em::Policy* policy_info,
                              const base::Value::Dict& policy) {
  if (!policy.contains(conflict_key)) {
    return;
  }
  for (const auto& conflicted_policy : *policy.FindList(conflict_key)) {
    UpdateConflictedPolicy(policy_info->add_conflicts(), conflicted_policy);
  }
}

void UpdatePolicyInfo(em::Policy* policy_info,
                      const std::string& policy_name,
                      const base::Value& policy) {
  policy_info->set_name(policy_name);
  policy_info->set_level(GetLevel(policy));
  policy_info->set_scope(GetScope(policy));
  policy_info->set_source(GetSource(policy));
  base::JSONWriter::Write(*policy.GetDict().Find("value"),
                          policy_info->mutable_value());
  const std::string* error = policy.GetDict().FindString("error");
  std::string deprecated_error;
  std::string future_error;
  // Because server side use keyword "deprecated" to determine policy
  // deprecation error. Using l10n string actually causing issue.
  if (policy.GetDict().FindBool("deprecated")) {
    deprecated_error = "This policy has been deprecated";
  }

  if (policy.GetDict().FindBool("future")) {
    future_error = "This policy hasn't been released";
  }

  if (error && !deprecated_error.empty())
    policy_info->set_error(
        base::JoinString({*error, deprecated_error, future_error}, "\n"));
  else if (error)
    policy_info->set_error(*error);
  else if (!deprecated_error.empty())
    policy_info->set_error(deprecated_error);

  UpdateConflictedPolicies("conflicts", policy_info, policy.GetDict());
  UpdateConflictedPolicies("superseded", policy_info, policy.GetDict());
}

}  // namespace

void AppendChromePolicyInfoIntoProfileReport(
    const base::Value::Dict& policies,
    em::ChromeUserProfileInfo* profile_info) {
  for (auto policy_iter : *policies.FindDict("chromePolicies")) {
    UpdatePolicyInfo(profile_info->add_chrome_policies(), policy_iter.first,
                     policy_iter.second);
  }
}

void AppendExtensionPolicyInfoIntoProfileReport(
    const base::Value::Dict& policies,
    em::ChromeUserProfileInfo* profile_info) {
  if (!policies.Find("extensionPolicies")) {
    // Android and iOS don't support extensions and their policies.
    return;
  }

  for (auto extension_iter : *policies.FindDict("extensionPolicies")) {
    const base::Value& policies_value = extension_iter.second;
    if (policies_value.GetDict().size() == 0) {
      continue;
    }

    auto* extension = profile_info->add_extension_policies();
    extension->set_extension_id(extension_iter.first);
    for (auto policy_iter : policies_value.GetDict()) {
      UpdatePolicyInfo(extension->add_policies(), policy_iter.first,
                       policy_iter.second);
    }
  }
}

void AppendCloudPolicyFetchTimestamp(em::ChromeUserProfileInfo* profile_info,
                                     policy::CloudPolicyManager* manager) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!manager || !manager->IsClientRegistered())
    return;
  auto* timestamp = profile_info->add_policy_fetched_timestamps();
  timestamp->set_timestamp(manager->core()
                               ->client()
                               ->last_policy_timestamp()
                               .InMillisecondsSinceUnixEpoch());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace enterprise_reporting
