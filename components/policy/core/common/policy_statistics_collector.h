// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_STATISTICS_COLLECTOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_STATISTICS_COLLECTOR_H_

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class TaskRunner;
}

namespace policy {

class PolicyService;

// Different types of policies
enum Condition {
  kDefault,
  kMandatory,
  kRecommended,
};

enum class PoliciesSources {
  kCloudOnly = 0,
  kCloudOnlyExceptEnrollment = 1,
  kPlatformOnly = 2,
  kHybrid = 3,
  kEnrollmentOnly = 4,
  kMaxValue = kEnrollmentOnly,
};

// Values for the BrowserSignin policy.
// VALUES MUST COINCIDE WITH THE BrowserSignin POLICY DEFINITION.
enum class BrowserSigninMode {
  kDisabled = 0,
  kEnabled = 1,
  kForced = 2,
  kMaxValue = kForced
};

// Manages regular updates of policy usage UMA histograms.
class POLICY_EXPORT PolicyStatisticsCollector {
 public:
  // Policy usage statistics update rate, in milliseconds.
  static const base::TimeDelta kStatisticsUpdateRate;

  // Neither |policy_service| nor |prefs| can be NULL and must stay valid
  // throughout the lifetime of PolicyStatisticsCollector.
  PolicyStatisticsCollector(const GetChromePolicyDetailsCallback& get_details,
                            const Schema& chrome_schema,
                            PolicyService* policy_service,
                            PrefService* prefs,
                            const scoped_refptr<base::TaskRunner>& task_runner);
  PolicyStatisticsCollector(const PolicyStatisticsCollector&) = delete;
  PolicyStatisticsCollector& operator=(const PolicyStatisticsCollector&) =
      delete;
  virtual ~PolicyStatisticsCollector();

  // Completes initialization and starts periodical statistic updates.
  void Initialize();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  void RecordPolicyUse(int id, Condition condition);
  void CollectStatistics();
  void ScheduleUpdate(base::TimeDelta delay);

  GetChromePolicyDetailsCallback get_details_;
  Schema chrome_schema_;
  raw_ptr<PolicyService> policy_service_;
  raw_ptr<PrefService> prefs_;

  base::CancelableOnceClosure update_callback_;

  const scoped_refptr<base::TaskRunner> task_runner_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_STATISTICS_COLLECTOR_H_
