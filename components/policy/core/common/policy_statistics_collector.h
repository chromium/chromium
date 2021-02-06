// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_STATISTICS_COLLECTOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_STATISTICS_COLLECTOR_H_

#include "base/cancelable_callback.h"
#include "base/macros.h"
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

// Manages regular updates of policy usage UMA histograms.
class POLICY_EXPORT PolicyStatisticsCollector {
 public:
  // Policy usage statistics update rate, in milliseconds.
  static const int kStatisticsUpdateRate;

  // Neither |policy_service| nor |prefs| can be NULL and must stay valid
  // throughout the lifetime of PolicyStatisticsCollector.
  PolicyStatisticsCollector(const GetChromePolicyDetailsCallback& get_details,
                            const Schema& chrome_schema,
                            PolicyService* policy_service,
                            PrefService* prefs,
                            const scoped_refptr<base::TaskRunner>& task_runner);
  virtual ~PolicyStatisticsCollector();

  // Completes initialization and starts periodical statistic updates.
  void Initialize();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // protected virtual for mocking.
  virtual void RecordPolicyUse(int id);
  virtual void RecordPolicyGroupWithConflicts(int id);
  virtual void RecordPolicyIgnoredByAtomicGroup(int id);

 private:
  void CollectStatistics();
  void ScheduleUpdate(base::TimeDelta delay);

  GetChromePolicyDetailsCallback get_details_;
  Schema chrome_schema_;
  PolicyService* policy_service_;
  PrefService* prefs_;

  base::CancelableOnceClosure update_callback_;

  const scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PolicyStatisticsCollector);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_STATISTICS_COLLECTOR_H_
