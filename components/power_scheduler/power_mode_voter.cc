// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_voter.h"

#include <limits>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace power_scheduler {
namespace {

bool IsBrowserProcess() {
  return base::CommandLine::ForCurrentProcess()
      ->GetSwitchValueASCII("type")
      .empty();
}

const char* GetFrameDamageAreaHistogramName() {
  return IsBrowserProcess() ? "Power.FrameDamageAreaInDip.Browser"
                            : "Power.FrameDamageAreaInDip.Renderer";
}

const char* GetFrameDamageDiagonalHistogramName() {
  return IsBrowserProcess() ? "Power.FrameDamageDiagonalInDip.Browser"
                            : "Power.FrameDamageDiagonalInDip.Renderer";
}

}  // namespace

PowerModeVoter::Delegate::~Delegate() = default;

PowerModeVoter::PowerModeVoter(Delegate* delegate) : delegate_(delegate) {}

// static
constexpr base::TimeDelta PowerModeVoter::kResponseTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kAnimationTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kVideoTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kLoadingTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kStuckLoadingTimeout;
// static
constexpr base::TimeDelta PowerModeVoter::kScriptExecutionTimeout;

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
    : FrameProductionPowerModeVoter(name, PowerModeArbiter::GetInstance()) {}

FrameProductionPowerModeVoter::FrameProductionPowerModeVoter(
    const char* name,
    PowerModeArbiter* arbiter)
    : voter_(arbiter->NewVoter(name)) {}

FrameProductionPowerModeVoter::~FrameProductionPowerModeVoter() = default;

void FrameProductionPowerModeVoter::OnNeedsBeginFramesChanged(
    bool needs_begin_frames) {
  needs_begin_frames_ = needs_begin_frames;
  if (needs_begin_frames) {
    in_nop_animation_ = false;
    consecutive_frames_skipped_ = 0;
    // Start out with a 2 x max frame damage history. Thus, we will require
    // |kNumDamageAreas-1| frames with lower damage area before voting for
    // small/medium animation modes.
    last_damage_areas_ = {std::numeric_limits<int>::max(),
                          std::numeric_limits<int>::max()};
    voter_->VoteFor(PowerMode::kAnimation);
  } else {
    voter_->ResetVoteAfterTimeout(PowerModeVoter::kAnimationTimeout);
  }
}

void FrameProductionPowerModeVoter::OnFrameProduced(
    const gfx::Rect& damage_rect,
    float device_scale_factor) {
  // Only consider produced frames when we still need BeginFrames.
  if (!needs_begin_frames_)
    return;

  // In DIP. Assuming 160 dip/inch, one square inch is 160*160 = 25600 dip. A
  // typical 6 inch screen has about 13.5 square inch area. We'll define
  // animations up to 3 square inch as "small" animation and 6 as "medium".
  static constexpr int kMaxDamageAreaForSmallAnimation = 3 * 160 * 160;
  static constexpr int kMaxDamageAreaForMediumAnimation = 6 * 160 * 160;

  PowerMode vote = PowerMode::kAnimation;

  gfx::Size damage_rect_in_dip;
  if (device_scale_factor > 0) {
    float inverse_dsf = 1 / device_scale_factor;
    damage_rect_in_dip =
        gfx::ScaleToCeiledSize(damage_rect.size(), inverse_dsf);
  }
  if (!damage_rect_in_dip.IsEmpty()) {
    int damage_area_in_dip = damage_rect_in_dip.GetCheckedArea().ValueOrDefault(
        std::numeric_limits<int>::max());
    int damage_diagonal_in_dip = std::ceil(
        gfx::Vector2d(damage_rect_in_dip.width(), damage_rect_in_dip.height())
            .Length());

    UMA_HISTOGRAM_COUNTS_10M(GetFrameDamageAreaHistogramName(),
                             damage_area_in_dip);
    UMA_HISTOGRAM_COUNTS_10000(GetFrameDamageDiagonalHistogramName(),
                               damage_diagonal_in_dip);

    // Record the area into last_damage_areas_ after shifting existing values.
    for (int i = kNumDamageAreas - 1; i >= 1; i--)
      last_damage_areas_[i] = last_damage_areas_[i - 1];
    last_damage_areas_[0] = damage_area_in_dip;

    // Determine the second largest area damaged over the last kNumDamageAreas
    // frames, ignoring the maximum. This way, we ignore one-off frames that
    // damage a large area but are submitted in between a sequence of smaller
    // animation frames.
    int max_damage_area = 0;
    int second_max_damage_area = 0;
    for (int damage_area : last_damage_areas_) {
      if (damage_area > max_damage_area) {
        second_max_damage_area = max_damage_area;
        max_damage_area = damage_area;
      } else if (damage_area > second_max_damage_area) {
        second_max_damage_area = damage_area;
      }
    }

    if (second_max_damage_area <= kMaxDamageAreaForSmallAnimation) {
      vote = PowerMode::kSmallAnimation;
    } else if (second_max_damage_area <= kMaxDamageAreaForMediumAnimation) {
      vote = PowerMode::kMediumAnimation;
    }
  }

  consecutive_frames_skipped_ = 0;

  // If we were in no-op mode, only go back into an animation mode if there were
  // at least two frames produced within kAnimationTimeout.
  base::TimeTicks now = base::TimeTicks::Now();
  if (in_nop_animation_ &&
      last_frame_produced_timestamp_ + PowerModeVoter::kAnimationTimeout <
          now) {
    last_frame_produced_timestamp_ = now;
    return;
  }

  in_nop_animation_ = false;
  last_frame_produced_timestamp_ = now;
  voter_->VoteFor(vote);
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

  if (consecutive_frames_skipped_ < kMinFramesSkippedForIdleAnimation - 1) {
    consecutive_frames_skipped_++;
    return;
  }

  in_nop_animation_ = true;
  last_frame_produced_timestamp_ = base::TimeTicks();
  voter_->VoteFor(PowerMode::kNopAnimation);
}

void FrameProductionPowerModeVoter::OnFrameTimeout() {
  in_nop_animation_ = true;
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
