// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_statistics_collector.h"

#include <algorithm>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

constexpr char kPoliciesSourceMetricsName[] = "Enterprise.Policies.Sources";
#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kBrowserSigninModeMetricsName[] =
    "Enterprise.BrowserSigninPolicy";
#endif

constexpr const char* kCBCMEnrollmentPolicies[] = {
    "CloudManagementEnrollmentToken", "CloudManagementEnrollmentMandatory"};

enum SimplePolicySource {
  kNone = 0,
  kCloud = 1 << 0,
  kPlatform = 1 << 1,
  kMerge = kCloud | kPlatform,
  kEnrollment = 1 << 2,
};

SimplePolicySource SimplifyPolicySource(PolicySource source,
                                        const std::string& policy_name) {
  switch (source) {
    case POLICY_SOURCE_CLOUD:
    case POLICY_SOURCE_CLOUD_FROM_ASH:
      return kCloud;
    case POLICY_SOURCE_PLATFORM:
    case POLICY_SOURCE_ACTIVE_DIRECTORY:
      // Adjust for enrollment policies which can never be set from cloud.
      // Count them as cloud policy so that a device is considered as cloud
      // managed even if there is enrollment token only.
      for (const char* enrollment_policy : kCBCMEnrollmentPolicies) {
        if (policy_name == enrollment_policy)
          return kEnrollment;
      }
      return kPlatform;
    case POLICY_SOURCE_MERGED:
      return kMerge;
    default:
      // Other sources are only used for speicial cases and will not be counted.
      return kNone;
  }
}

void RecordPoliciesSources(SimplePolicySource source) {
  if ((source & kMerge) == kMerge) {
    base::UmaHistogramEnumeration(kPoliciesSourceMetricsName,
                                  PoliciesSources::kHybrid);
  } else if (source & kPlatform) {
    base::UmaHistogramEnumeration(kPoliciesSourceMetricsName,
                                  PoliciesSources::kPlatformOnly);
  } else if (source & kCloud) {
    if (source & kEnrollment) {
      base::UmaHistogramEnumeration(
          kPoliciesSourceMetricsName,
          PoliciesSources::kCloudOnlyExceptEnrollment);
    } else {
      base::UmaHistogramEnumeration(kPoliciesSourceMetricsName,
                                    PoliciesSources::kCloudOnly);
    }
  } else if (source & kEnrollment) {
    base::UmaHistogramEnumeration(kPoliciesSourceMetricsName,
                                  PoliciesSources::kEnrollmentOnly);
  }
}
#if !BUILDFLAG(IS_CHROMEOS)
// Records UMA metrics for signin mode
void RecordBrowserSigninMode(const base::Value* value) {
  if (value && value->is_int() && 0 <= value->GetInt() &&
      value->GetInt() <= static_cast<int>(BrowserSigninMode::kMaxValue)) {
    base::UmaHistogramEnumeration(
        kBrowserSigninModeMetricsName,
        static_cast<BrowserSigninMode>(value->GetInt()));
  };
}
#endif
}  // namespace

const base::TimeDelta PolicyStatisticsCollector::kStatisticsUpdateRate =
    base::Days(1);

PolicyStatisticsCollector::PolicyStatisticsCollector(
    const GetChromePolicyDetailsCallback& get_details,
    const Schema& chrome_schema,
    PolicyService* policy_service,
    PrefService* prefs,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : get_details_(get_details),
      chrome_schema_(chrome_schema),
      policy_service_(policy_service),
      prefs_(prefs),
      task_runner_(task_runner) {}

PolicyStatisticsCollector::~PolicyStatisticsCollector() = default;

void PolicyStatisticsCollector::Initialize() {
  base::Time last_update =
      prefs_->GetTime(policy_prefs::kLastPolicyStatisticsUpdate);
  base::TimeDelta delay =
      std::max(base::Time::Now() - last_update, base::TimeDelta());
  if (delay >= kStatisticsUpdateRate)
    CollectStatistics();
  else
    ScheduleUpdate(kStatisticsUpdateRate - delay);
}

// static
void PolicyStatisticsCollector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(policy_prefs::kLastPolicyStatisticsUpdate, 0);
}

void PolicyStatisticsCollector::RecordPolicyUse(int id, Condition condition) {
  std::string suffix;
  switch (condition) {
    case kDefault:
      break;
    case kMandatory:
      suffix = ".Mandatory";
      break;
    case kRecommended:
      suffix = ".Recommended";
      break;
  }
  base::UmaHistogramSparse("Enterprise.Policies" + suffix, id);
}

void PolicyStatisticsCollector::CollectStatistics() {
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  int source = kNone;
  // Collect statistics.
  for (Schema::Iterator it(chrome_schema_.GetPropertiesIterator());
       !it.IsAtEnd(); it.Advance()) {
    const PolicyMap::Entry* policy_entry = policies.Get(it.key());
    if (!policy_entry)
      continue;
    const PolicyDetails* details = get_details_.Run(it.key());
    if (details) {
      RecordPolicyUse(details->id, kDefault);
      if (policies.Get(it.key())->level == POLICY_LEVEL_MANDATORY) {
        RecordPolicyUse(details->id, kMandatory);
      } else {
        RecordPolicyUse(details->id, kRecommended);
      }
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    source |= SimplifyPolicySource(policy_entry->source, it.key());
  }

  RecordPoliciesSources(static_cast<SimplePolicySource>(source));
#if !BUILDFLAG(IS_CHROMEOS)
  RecordBrowserSigninMode(
      policies.GetValue(key::kBrowserSignin, base::Value::Type::INTEGER));
#endif

  // Take care of next update.
  prefs_->SetTime(policy_prefs::kLastPolicyStatisticsUpdate, base::Time::Now());
  ScheduleUpdate(kStatisticsUpdateRate);
}

void PolicyStatisticsCollector::ScheduleUpdate(base::TimeDelta delay) {
  update_callback_.Reset(base::BindOnce(
      &PolicyStatisticsCollector::CollectStatistics, base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, update_callback_.callback(), delay);
}

}  // namespace policy
