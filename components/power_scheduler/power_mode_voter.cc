// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_voter.h"

#include "components/power_scheduler/power_mode_arbiter.h"

namespace power_scheduler {

PowerModeVoter::Delegate::~Delegate() = default;

PowerModeVoter::PowerModeVoter(Delegate* delegate) : delegate_(delegate) {}

// static
constexpr base::TimeDelta PowerModeVoter::kResponseTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kAnimationTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kVideoTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kSoftwareDrawTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kLoadingTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kStuckLoadingTimeout;

PowerModeVoter::~PowerModeVoter() {
  delegate_->OnVoterDestroyed(this);
}

void PowerModeVoter::VoteFor(PowerMode mode) {
  delegate_->SetVote(this, mode);
}

void PowerModeVoter::ResetVoteAfterTimeout(base::TimeDelta timeout) {
  delegate_->ResetVoteAfterTimeout(this, timeout);
}

// static
constexpr int FrameProductionPowerModeVoter::kMinFramesSkippedForIdleAnimation;

FrameProductionPowerModeVoter::FrameProductionPowerModeVoter(const char* name)
    : voter_(PowerModeArbiter::GetInstance()->NewVoter(name)) {}

FrameProductionPowerModeVoter::~FrameProductionPowerModeVoter() = default;

void FrameProductionPowerModeVoter::OnNeedsBeginFramesChanged(
    bool needs_begin_frames) {
  needs_begin_frames_ = needs_begin_frames;
  if (needs_begin_frames) {
    consecutive_frames_skipped_ = 0;
    voter_->VoteFor(PowerMode::kAnimation);
  } else {
    voter_->ResetVoteAfterTimeout(PowerModeVoter::kAnimationTimeout);
  }
}

void FrameProductionPowerModeVoter::OnFrameProduced() {
  consecutive_frames_skipped_ = 0;

  // If we were in no-op mode, only go back into animation mode if there were at
  // least two frames produced within kAnimationTimeout.
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_frame_produced_timestamp_ + PowerModeVoter::kAnimationTimeout <
      now) {
    last_frame_produced_timestamp_ = now;
    return;
  }

  last_frame_produced_timestamp_ = now;
  voter_->VoteFor(PowerMode::kAnimation);

  if (!needs_begin_frames_)
    voter_->ResetVoteAfterTimeout(PowerModeVoter::kAnimationTimeout);
}

void FrameProductionPowerModeVoter::OnFrameSkipped(bool frame_completed,
                                                   bool waiting_on_main) {
  // Only consider skipped frames when we still need BeginFrames.
  if (!needs_begin_frames_)
    return;

  // Ignore frames that are skipped in an incomplete state, e.g. because frame
  // production took too long and the deadline was missed. Such frames should
  // not count as "no-op", because frame production may still be in progress.
  // However, if we were only waiting on the main thread, we will treat this as
  // no-op here, because we cannot distinguish aborted BeginMainFrame sequences
  // from "long" content-producing BeginMainFrames here. Instead, a separate
  // PowerModeVoter tracks BeginMainFrame production in cc::Scheduler.
  if (!frame_completed && !waiting_on_main)
    return;

  if (consecutive_frames_skipped_ < kMinFramesSkippedForIdleAnimation) {
    consecutive_frames_skipped_++;
    return;
  }

  voter_->VoteFor(PowerMode::kNopAnimation);
}

void FrameProductionPowerModeVoter::OnFrameTimeout() {
  voter_->VoteFor(PowerMode::kNopAnimation);
}

DebouncedPowerModeVoter::DebouncedPowerModeVoter(const char* name,
                                                 base::TimeDelta timeout)
    : voter_(PowerModeArbiter::GetInstance()->NewVoter(name)),
      timeout_(timeout) {}

DebouncedPowerModeVoter::~DebouncedPowerModeVoter() = default;

void DebouncedPowerModeVoter::VoteFor(PowerMode vote) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_vote_ || vote != *last_vote_ ||
      last_vote_timestamp_ + timeout_ < now) {
    last_vote_ = vote;
    last_vote_timestamp_ = now;
    return;
  }

  DCHECK_EQ(*last_vote_, vote);
  last_vote_timestamp_ = now;
  voter_->VoteFor(vote);
}

}  // namespace power_scheduler
