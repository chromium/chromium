// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_MOBILE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_MOBILE_H_

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/variations/variations_request_scheduler.h"

class PrefService;

namespace variations {

// A specialized VariationsRequestScheduler that manages request cycles for the
// VariationsService on mobile platforms.
class COMPONENT_EXPORT(VARIATIONS) VariationsRequestSchedulerMobile
    : public VariationsRequestScheduler {
 public:
  // |task| is the closure to call when the scheduler deems ready. |local_state|
  // is the PrefService that contains the time of the last fetch.
  VariationsRequestSchedulerMobile(const base::RepeatingClosure& task,
                                   PrefService* local_state);

  VariationsRequestSchedulerMobile(const VariationsRequestSchedulerMobile&) =
      delete;
  VariationsRequestSchedulerMobile& operator=(
      const VariationsRequestSchedulerMobile&) = delete;

  ~VariationsRequestSchedulerMobile() override;

  // VariationsRequestScheduler:
  void Start() override;
  void Reset() override;
  void OnAppEnterForeground() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsRequestSchedulerMobileTest,
                           OnAppEnterForegroundNoRun);
  FRIEND_TEST_ALL_PREFIXES(VariationsRequestSchedulerMobileTest,
                           OnAppEnterForegroundRun);
  FRIEND_TEST_ALL_PREFIXES(VariationsRequestSchedulerMobileTest,
                           OnAppEnterForegroundOnStartup);

  // Determines whether we should fetch a seed depending on how much time has
  // passed since the last seed fetch. If the
  // |kDisableVariationsSeedFetchThrottling| command line switch is present,
  // this always returns true.
  bool ShouldFetchSeed(base::Time last_fetch_time);

  // The local state instance that provides the last fetch time.
  raw_ptr<PrefService> local_state_;

  // Timer used for triggering a delayed fetch for ScheduleFetch().
  base::OneShotTimer schedule_fetch_timer_;

  // The time the last seed request was initiated.
  base::Time last_request_time_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_MOBILE_H_
