// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_MODE_VOTER_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_MODE_VOTER_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "components/power_scheduler/power_mode.h"

namespace power_scheduler {

class PowerModeArbiter;

// PowerModeVoters should be instantiated at instrumentation points in Chromium
// via PowerModeArbiter::GetInstance()->NewVoter("MyVoter") to vote on a
// process's PowerMode.
class COMPONENT_EXPORT(POWER_SCHEDULER) PowerModeVoter {
 public:
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void OnVoterDestroyed(PowerModeVoter*) = 0;
    virtual void SetVote(PowerModeVoter*, PowerMode) = 0;
    virtual void ResetVoteAfterTimeout(PowerModeVoter*,
                                       base::TimeDelta timeout) = 0;
  };

  // Consider an initial response to a single input to last 100ms.
  static constexpr base::TimeDelta kResponseTimeout =
      base::TimeDelta::FromMilliseconds(100);

  // Animations often have brief idle periods where no frames are produced. This
  // timeout is applied before resetting animation votes to avoid frequent vote
  // reversals.
  static constexpr base::TimeDelta kAnimationTimeout =
      base::TimeDelta::FromMilliseconds(50);

  // Avoid getting stuck in loading stage forever. More than 99.9% of
  // navigations load (to largest contentful paint) in less than a minute.
  static constexpr base::TimeDelta kLoadingTimeout =
      base::TimeDelta::FromSeconds(60);

  ~PowerModeVoter();

  PowerModeVoter(const PowerModeVoter&) = delete;
  PowerModeVoter& operator=(const PowerModeVoter&) = delete;

  // Set the voter's vote to the provided PowerMode. Cancels any previously
  // scheduled ResetVoteAfterTimeout().
  void VoteFor(PowerMode);

  // Resets the vote to the PowerMode::kIdle after |timeout|, provided VoteFor()
  // is not called again before then.
  void ResetVoteAfterTimeout(base::TimeDelta timeout);

 private:
  friend class PowerModeArbiter;
  explicit PowerModeVoter(Delegate* delegate);

  Delegate* delegate_;
};

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_MODE_VOTER_H_
