// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/surface_animation_manager.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"

namespace viz {
SurfaceAnimationManager::SurfaceAnimationManager() = default;
SurfaceAnimationManager::~SurfaceAnimationManager() = default;

bool SurfaceAnimationManager::ProcessTransitionDirectives(
    base::TimeTicks last_frame_time,
    const std::vector<CompositorFrameTransitionDirective>& directives,
    SurfaceSavedFrameStorage* storage) {
  DCHECK_GE(last_frame_time, current_time_);
  current_time_ = last_frame_time;
  bool started_animation = false;
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
        started_animation |= ProcessAnimateDirective(directive, storage);
        break;
    }
  }
  return started_animation;
}

void SurfaceAnimationManager::ProcessSaveDirective(
    const CompositorFrameTransitionDirective& directive,
    SurfaceSavedFrameStorage* storage) {
  // We need to be in the idle state in order to save.
  if (state_ != State::kIdle)
    return;
  storage->ProcessSaveDirective(directive);
}

bool SurfaceAnimationManager::ProcessAnimateDirective(
    const CompositorFrameTransitionDirective& directive,
    SurfaceSavedFrameStorage* storage) {
  // We can only begin an animate if we are currently idle.
  if (state_ != State::kIdle)
    return false;

  // Make sure we don't actually have anything saved as a texture.
  DCHECK(!saved_root_texture_.has_value());

  auto saved_frame = storage->TakeSavedFrame();
  // We can't animate if we don't have a saved frame.
  if (!saved_frame || !saved_frame->IsValid())
    return false;

  // Convert the texture result into a transferable resource.
  saved_directive_ = saved_frame->directive();
  saved_root_texture_.emplace(
      transferable_resource_tracker_.ImportResource(std::move(saved_frame)));

  state_ = State::kAnimating;
  started_time_ = current_time_;
  return true;
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
  DCHECK(saved_root_texture_.has_value());
  if (current_time_ >= started_time_ + saved_directive_.duration())
    state_ = State::kDone;
}

void SurfaceAnimationManager::FinalizeAndDisposeOfState() {
  DCHECK_EQ(state_, State::kDone);
  DCHECK(saved_root_texture_.has_value());
  // Set state to idle.
  state_ = State::kIdle;

  // Ensure to return the texture / unref it.
  transferable_resource_tracker_.UnrefResource(saved_root_texture_->id);
  saved_root_texture_.reset();

  // Other resets so that we don't accidentally access stale values.
  saved_directive_ = CompositorFrameTransitionDirective();
  started_time_ = base::TimeTicks();
}

double SurfaceAnimationManager::CalculateAnimationProgress() const {
  DCHECK(state_ == State::kAnimating || state_ == State::kDone);
  if (state_ == State::kDone)
    return 1.;

  double result = (current_time_ - started_time_) / saved_directive_.duration();
  DCHECK_GE(result, 0.);
  DCHECK_LE(result, 1.);
  return result;
}

void SurfaceAnimationManager::InterpolateFrame(Surface* surface) {
  // TODO(vmpstr): Do the interpolation and saving things back to surface.
}

void SurfaceAnimationManager::RefResources(
    const std::vector<TransferableResource>& resources) {
  if (transferable_resource_tracker_.is_empty())
    return;
  for (const auto& resource : resources) {
    if (resource.id >= kVizReservedRangeStartId)
      transferable_resource_tracker_.RefResource(resource.id);
  }
}

void SurfaceAnimationManager::UnrefResources(
    const std::vector<ReturnedResource>& resources) {
  if (transferable_resource_tracker_.is_empty())
    return;
  for (const auto& resource : resources) {
    if (resource.id >= kVizReservedRangeStartId)
      transferable_resource_tracker_.UnrefResource(resource.id);
  }
}

}  // namespace viz
