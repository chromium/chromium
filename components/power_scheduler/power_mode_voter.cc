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

PowerModeVoter::~PowerModeVoter() {
  delegate_->OnVoterDestroyed(this);
}

void PowerModeVoter::VoteFor(PowerMode mode) {
  delegate_->SetVote(this, mode);
}

void PowerModeVoter::ResetVoteAfterTimeout(base::TimeDelta timeout) {
  delegate_->ResetVoteAfterTimeout(this, timeout);
}

FrameProductionPowerModeVoter::FrameProductionPowerModeVoter(const char* name)
    : voter_(PowerModeArbiter::GetInstance()->NewVoter(name)) {}

FrameProductionPowerModeVoter::~FrameProductionPowerModeVoter() = default;

void FrameProductionPowerModeVoter::OnNeedsBeginFramesChanged(
    bool needs_begin_frames) {
  if (needs_begin_frames) {
    consecutive_frames_skipped_ = 0;
    voter_->VoteFor(PowerMode::kAnimation);
  } else {
    voter_->ResetVoteAfterTimeout(PowerModeVoter::kAnimationTimeout);
  }
}

void FrameProductionPowerModeVoter::OnFrameProduced() {
  consecutive_frames_skipped_ = 0;
  voter_->VoteFor(PowerMode::kAnimation);
}

void FrameProductionPowerModeVoter::OnFrameSkipped(bool frame_completed,
                                                   bool waiting_on_main) {
  // Ignore frames that are skipped in an incomplete state, e.g. because frame
  // production took too long and the deadline was missed. Such frames should
  // not count as "no-op", because frame production may still be in progress.
  // However, if we were only waiting on the main thread, we will treat this as
  // no-op here, because we cannot distinguish aborted BeginMainFrame sequences
  // from "long" content-producing BeginMainFrames here. Instead, a separate
  // PowerModeVoter tracks BeginMainFrame production in cc::Scheduler.
  if (!frame_completed && !waiting_on_main)
    return;

  static constexpr int kMinFramesSkippedForIdleAnimation = 4;

  if (consecutive_frames_skipped_ < kMinFramesSkippedForIdleAnimation) {
    consecutive_frames_skipped_++;
    return;
  }

  voter_->VoteFor(PowerMode::kNopAnimation);
}

}  // namespace power_scheduler
