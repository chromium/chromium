// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame_storage.h"

#include <memory>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"

namespace viz {
namespace {

// Expire saved frames after 5 seconds.
// TODO(vmpstr): Figure out if we need to change this for cross-origin
// animations, since the network delay can cause us to wait longer.
constexpr base::TimeDelta kExpiryTime = base::TimeDelta::FromSeconds(5);

}  // namespace

SurfaceSavedFrameStorage::SurfaceSavedFrameStorage(Surface* surface)
    : surface_(surface) {}

SurfaceSavedFrameStorage::~SurfaceSavedFrameStorage() = default;

void SurfaceSavedFrameStorage::ProcessSaveDirective(
    const CompositorFrameTransitionDirective& directive) {
  // Create a new saved frame, destroying the old one if it existed.
  // TODO(vmpstr): This may need to change if the directive refers to a local
  // subframe (RP) of the compositor frame. However, as of now, the save
  // directive can only reference the root render pass.
  saved_frame_ = std::make_unique<SurfaceSavedFrame>(directive);

  // Schedule an expiry callback.
  // Note that since the expiry_closure_ has a shorter lifetime than `this`, we
  // bind `this` as unretained.
  expiry_closure_.Reset(base::BindOnce(
      &SurfaceSavedFrameStorage::ExpireSavedFrame, base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, expiry_closure_.callback(), kExpiryTime);

  // TODO(vmpstr): Ask surface for the active compositor frame and give it to
  // `saved_frame_` so that it can add copy output requests for whatever is
  // needed.
  // For now use surface to silence the compiler warnings.
  (void)surface_;
}

std::unique_ptr<SurfaceSavedFrame> SurfaceSavedFrameStorage::TakeSavedFrame() {
  // If the saved frame is not valid, then we should reset it. An example of an
  // invalid frame is a frame that never received a response to the scheduled
  // copy output requests.
  if (!saved_frame_ || !saved_frame_->IsValid())
    saved_frame_.reset();
  expiry_closure_.Cancel();
  return std::move(saved_frame_);
}

void SurfaceSavedFrameStorage::ExpireSavedFrame() {
  saved_frame_.reset();
}

}  // namespace viz
