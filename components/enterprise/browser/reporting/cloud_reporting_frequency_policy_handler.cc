// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/cloud_reporting_frequency_policy_handler.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_reporting {
namespace {
constexpr int kMinimumReportFrequencyInHour = 3;
constexpr int kMaximumReportFrequencyInour = 24;

}  // namespace

CloudReportingFrequencyPolicyHandler::CloudReportingFrequencyPolicyHandler()
    : policy::IntRangePolicyHandlerBase(
          policy::key::kCloudReportingUploadFrequency,
          kMinimumReportFrequencyInHour,
          kMaximumReportFrequencyInour,
          /*clamp=*/true) {}
CloudReportingFrequencyPolicyHandler::~CloudReportingFrequencyPolicyHandler() =
    default;

bool CloudReportingFrequencyPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());

  if (!policy)
    return true;

  if (policy->source != policy::POLICY_SOURCE_CLOUD &&
      policy->source != policy::POLICY_SOURCE_CLOUD_FROM_ASH) {
    errors->AddError(policy_name(), IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR);
    return false;
  }

  return policy::IntRangePolicyHandlerBase::CheckPolicySettings(policies,
                                                                errors);
}

void CloudReportingFrequencyPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr))
    prefs->SetValue(kCloudReportingUploadFrequency,
                    base::TimeDeltaToValue(base::Hours(value_in_range)));
}

}  // namespace enterprise_reporting
