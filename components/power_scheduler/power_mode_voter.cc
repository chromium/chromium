// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_voter.h"

namespace power_scheduler {

PowerModeVoter::Delegate::~Delegate() = default;

PowerModeVoter::PowerModeVoter(Delegate* delegate) : delegate_(delegate) {}

// static
constexpr base::TimeDelta PowerModeVoter::kResponseTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kAnimationTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kLoadingTimeout;

PowerModeVoter::~PowerModeVoter() {
  delegate_->OnVoterDestroyed(this);
}

void PowerModeVoter::VoteFor(PowerMode mode) {
  delegate_->SetVote(this, mode);
}

void PowerModeVoter::ResetVoteAfterTimeout(base::TimeDelta timeout) {
  delegate_->ResetVoteAfterTimeout(this, timeout);
}

}  // namespace power_scheduler
