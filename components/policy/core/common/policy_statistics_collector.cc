// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_statistics_collector.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/task_runner.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

const int PolicyStatisticsCollector::kStatisticsUpdateRate =
    24 * 60 * 60 * 1000;  // 24 hours.

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
      task_runner_(task_runner) {
}

PolicyStatisticsCollector::~PolicyStatisticsCollector() {
}

void PolicyStatisticsCollector::Initialize() {
  using base::Time;

  base::TimeDelta update_rate = base::Milliseconds(kStatisticsUpdateRate);
  Time last_update = prefs_->GetTime(policy_prefs::kLastPolicyStatisticsUpdate);
  base::TimeDelta delay = std::max(Time::Now() - last_update, base::Days(0));
  if (delay >= update_rate)
    CollectStatistics();
  else
    ScheduleUpdate(update_rate - delay);
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
    case kIgnoredByAtomicGroup:
      suffix = ".IgnoredByPolicyGroup";
      break;
  }
  base::UmaHistogramSparse("Enterprise.Policies" + suffix, id);
}

void PolicyStatisticsCollector::CollectStatistics() {
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  // Collect statistics.
  for (Schema::Iterator it(chrome_schema_.GetPropertiesIterator());
       !it.IsAtEnd(); it.Advance()) {
    if (policies.Get(it.key())) {
      const PolicyDetails* details = get_details_.Run(it.key());
      if (details) {
        RecordPolicyUse(details->id, kDefault);
        if (policies.Get(it.key())->level == POLICY_LEVEL_MANDATORY) {
          RecordPolicyUse(details->id, kMandatory);
        } else {
          RecordPolicyUse(details->id, kRecommended);
        }
      } else {
        NOTREACHED();
      }
    }
  }

  for (size_t i = 0; i < kPolicyAtomicGroupMappingsLength; ++i) {
    const AtomicGroup& group = kPolicyAtomicGroupMappings[i];
    // Find the policy with the highest priority that is both in |policies|
    // and |group.policies|, an array ending with a nullptr.
    for (const char* const* policy_name = group.policies; *policy_name;
         ++policy_name) {
      if (policies.IsPolicyIgnoredByAtomicGroup(*policy_name)) {
        const PolicyDetails* details = get_details_.Run(*policy_name);
        if (details)
          RecordPolicyUse(details->id, kIgnoredByAtomicGroup);
        else
          NOTREACHED();
      }
    }
  }

  // Take care of next update.
  prefs_->SetTime(policy_prefs::kLastPolicyStatisticsUpdate, base::Time::Now());
  ScheduleUpdate(base::Milliseconds(kStatisticsUpdateRate));
}

void PolicyStatisticsCollector::ScheduleUpdate(base::TimeDelta delay) {
  update_callback_.Reset(base::BindOnce(
      &PolicyStatisticsCollector::CollectStatistics, base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, update_callback_.callback(), delay);
}

}  // namespace policy