// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/surface_animation_manager.h"

#include <vector>

#include "base/time/time.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"

namespace viz {
SurfaceAnimationManager::SurfaceAnimationManager() = default;
SurfaceAnimationManager::~SurfaceAnimationManager() = default;

void SurfaceAnimationManager::ProcessTransitionDirectives(
    base::TimeTicks last_frame_time,
    const std::vector<CompositorFrameTransitionDirective>& directives,
    SurfaceSavedFrameStorage* storage) {
  DCHECK_GE(last_frame_time, current_time_);
  current_time_ = last_frame_time;
  for (auto& directive : directives) {
    // Don't process directives with sequence ids smaller than or equal to the
    // last seen one. It is possible that we call this with the same frame
    // multiple times.
    if (directive.sequence_id() <= last_processed_sequence_id_)
      continue;
    last_processed_sequence_id_ = directive.sequence_id();

    // Dispatch to a specialized function based on type.
    switch (directive.type()) {
      case CompositorFrameTransitionDirective::Type::kSave:
        ProcessSaveDirective(directive, storage);
        break;
      case CompositorFrameTransitionDirective::Type::kAnimate:
        ProcessAnimateDirective(directive, storage);
        break;
    }
  }
}

void SurfaceAnimationManager::ProcessSaveDirective(
    const CompositorFrameTransitionDirective& directive,
    SurfaceSavedFrameStorage* storage) {
  // We need to be in the idle state in order to save.
  if (state_ != State::kIdle)
    return;
  storage->ProcessSaveDirective(directive);
}

void SurfaceAnimationManager::ProcessAnimateDirective(
    const CompositorFrameTransitionDirective& directive,
    SurfaceSavedFrameStorage* storage) {
  // We can only begin an animate if we are currently idle.
  if (state_ != State::kIdle)
    return;

  saved_frame_ = storage->TakeSavedFrame();
  // We can't animate if we don't have a saved frame.
  if (!saved_frame_)
    return;

  state_ = State::kAnimating;
  started_time_ = current_time_;
}

bool SurfaceAnimationManager::NeedsBeginFrame() const {
  // If we're animating we need to keep pumping frames to advance the animation.
  // If we're done, we require one more frame to switch back to idle state.
  return state_ == State::kAnimating || state_ == State::kDone;
}

void SurfaceAnimationManager::NotifyFrameAdvanced(base::TimeTicks new_time) {
  DCHECK_GE(new_time, current_time_);
  current_time_ = new_time;
  switch (state_) {
    case State::kIdle:
      NOTREACHED() << "We should not advance frames when idle";
      break;
    case State::kAnimating:
      FinishAnimationIfNeeded();
      break;
    case State::kDone:
      FinalizeAndDisposeOfState();
      break;
  }
}

void SurfaceAnimationManager::FinishAnimationIfNeeded() {
  DCHECK_EQ(state_, State::kAnimating);
  DCHECK(saved_frame_);
  if (current_time_ >= started_time_ + saved_frame_->animation_duration())
    state_ = State::kDone;
}

void SurfaceAnimationManager::FinalizeAndDisposeOfState() {
  DCHECK_EQ(state_, State::kDone);
  state_ = State::kIdle;
  saved_frame_.reset();
  started_time_ = base::TimeTicks();
}

double SurfaceAnimationManager::CalculateAnimationProgress() const {
  DCHECK(state_ == State::kAnimating || state_ == State::kDone);
  DCHECK(saved_frame_);
  if (state_ == State::kDone)
    return 1.;

  double result =
      (current_time_ - started_time_) / saved_frame_->animation_duration();
  DCHECK_GE(result, 0.);
  DCHECK_LE(result, 1.);
  return result;
}

}  // namespace viz
