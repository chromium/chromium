// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/cloud_profile_reporting_policy_handler.h"

#include "base/command_line.h"
#include "base/values.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_reporting {
namespace {
constexpr char kAllowProfileReportingSetFromAllSources[] =
    "allow-profile-reporting-set-from-all-sources";
}

CloudProfileReportingPolicyHandler::CloudProfileReportingPolicyHandler()
    : policy::TypeCheckingPolicyHandler(
          policy::key::kCloudProfileReportingEnabled,
          base::Value::Type::BOOLEAN) {}

CloudProfileReportingPolicyHandler::~CloudProfileReportingPolicyHandler() =
    default;

bool CloudProfileReportingPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());
  if (!policy)
    return true;

  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAllowProfileReportingSetFromAllSources)) {
    if (policy->source != policy::POLICY_SOURCE_CLOUD ||
        policy->scope != policy::POLICY_SCOPE_USER) {
      errors->AddError(policy_name(), IDS_POLICY_CLOUD_USER_ONLY_ERROR);
      return false;
    }
  }

  return true;
}

void CloudProfileReportingPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* cloud_profile_reporting_policy_value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);

  if (cloud_profile_reporting_policy_value) {
    prefs->SetBoolean(kCloudProfileReportingEnabled,
                      cloud_profile_reporting_policy_value->GetBool());
  }
}

}  // namespace enterprise_reporting
