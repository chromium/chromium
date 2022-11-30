// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_MODE_VOTER_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_MODE_VOTER_H_

#include <array>
#include <limits>
#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/power_scheduler/power_mode.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

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
  static constexpr base::TimeDelta kResponseTimeout = base::Milliseconds(100);

  // Animations often have brief idle periods where no frames are produced. This
  // timeout is applied before resetting animation votes to avoid frequent vote
  // reversals.
  static constexpr base::TimeDelta kAnimationTimeout = base::Milliseconds(100);
  static constexpr base::TimeDelta kVideoTimeout = kAnimationTimeout;

  // Give frames an extra second to draw & settle after load completion.
  static constexpr base::TimeDelta kLoadingTimeout = base::Seconds(1);
  // Avoid getting stuck in loading stage forever. More than 99.9% of
  // navigations load (to largest contentful paint) in less than a minute.
  static constexpr base::TimeDelta kStuckLoadingTimeout = base::Seconds(60);

  // This timeout is applied before resetting script execution votes to avoid
  // frequent vote reversals.
  static constexpr base::TimeDelta kScriptExecutionTimeout =
      base::Milliseconds(50);

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

  raw_ptr<Delegate> delegate_;
};

// Tracks the BeginFrame signal as well as produced and skipped frames to vote
// either for the kAnimation, kNopAnimation, or kIdle modes.
class COMPONENT_EXPORT(POWER_SCHEDULER) FrameProductionPowerModeVoter {
 public:
  explicit FrameProductionPowerModeVoter(const char* name);
  ~FrameProductionPowerModeVoter();

  FrameProductionPowerModeVoter(const FrameProductionPowerModeVoter&) = delete;
  FrameProductionPowerModeVoter& operator=(
      const FrameProductionPowerModeVoter&) = delete;

  // Should be called when starting or stoping observing BeginFrames.
  void OnNeedsBeginFramesChanged(bool needs_begin_frames);
  // Should be called when a frame is produced.
  void OnFrameProduced(const gfx::Rect& damage_rect, float device_scale_factor);
  // Should be called when a frame is skipped. |frame_completed| should be true
  // if the frame production resulted in no visible updates and was completed on
  // time. In other cases (e.g. if the deadline was missed and frame production
  // continues for the next vsync), it should be false. |waiting_on_main| should
  // be true if the frame was not completed because the main thread's frame
  // production was not finished on time for the deadline.
  void OnFrameSkipped(bool frame_completed, bool waiting_on_main);
  // Should be called when BeginFrame was not followed by a draw within a set
  // timeframe.
  void OnFrameTimeout();

 private:
  friend class PowerModeArbiterFrameProductionTest;

  // For testing.
  FrameProductionPowerModeVoter(const char* name, PowerModeArbiter*);

  // 10 Frames: 166ms on 60fps, 111ms on 90fps, 83ms on 120fps. This should be a
  // reasonable compromise to avoid frequent flip-flopping between different
  // animation modes.
  static constexpr int kNumDamageAreas = 10;
  static constexpr int kMinFramesSkippedForIdleAnimation = 10;

  std::unique_ptr<PowerModeVoter> voter_;
  bool in_nop_animation_ = false;
  int consecutive_frames_skipped_ = 0;
  base::TimeTicks last_frame_produced_timestamp_;
  bool needs_begin_frames_ = false;
  std::array<int, kNumDamageAreas> last_damage_areas_ = {
      std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
};

// PowerModeVoter that requires two consecutive votes for the same PowerMode
// within a given timeout to move out of idle.
class COMPONENT_EXPORT(POWER_SCHEDULER) DebouncedPowerModeVoter {
 public:
  DebouncedPowerModeVoter(const char* name, base::TimeDelta timeout);
  ~DebouncedPowerModeVoter();

  void VoteFor(PowerMode vote);

  void ResetVoteAfterTimeout() { voter_->ResetVoteAfterTimeout(timeout_); }

 private:
  std::unique_ptr<PowerModeVoter> voter_;
  const base::TimeDelta timeout_;

  absl::optional<PowerMode> last_vote_;
  base::TimeTicks last_vote_timestamp_;
};

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_MODE_VOTER_H_
