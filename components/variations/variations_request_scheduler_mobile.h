// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_MOBILE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_MOBILE_H_

#include "base/bind.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/variations/variations_request_scheduler.h"

class PrefService;

namespace variations {

// A specialized VariationsRequestScheduler that manages request cycles for
// VariationsService on mobile platforms.
class VariationsRequestSchedulerMobile : public VariationsRequestScheduler {
 public:
  // |task} is the closure to call when the scheduler deems ready. |local_state|
  // is the PrefService that contains the time of the last fetch.
  explicit VariationsRequestSchedulerMobile(const base::RepeatingClosure& task,
                                            PrefService* local_state);
  ~VariationsRequestSchedulerMobile() override;

  // Base class overrides.
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

  // The local state instance that provides the last fetch time.
  PrefService* local_state_;

  // Timer used for triggering a delayed fetch for ScheduleFetch().
  base::OneShotTimer schedule_fetch_timer_;

  // The time the last seed request was initiated.
  base::Time last_request_time_;

  DISALLOW_COPY_AND_ASSIGN(VariationsRequestSchedulerMobile);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_MOBILE_H_
