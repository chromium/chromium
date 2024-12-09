// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/fling_scheduler_android.h"

#include "components/input/fling_controller.h"
#include "components/input/render_input_router.h"

namespace viz {

FlingSchedulerAndroid::FlingSchedulerAndroid(input::RenderInputRouter* rir,
                                             Delegate* delegate,
                                             const FrameSinkId& frame_sink_id)
    : rir_(*rir), delegate_(*delegate), frame_sink_id_(frame_sink_id) {}

FlingSchedulerAndroid::~FlingSchedulerAndroid() {
  StopObservingBeginFrames();
}

void FlingSchedulerAndroid::ScheduleFlingProgress(
    base::WeakPtr<input::FlingController> fling_controller) {
  DCHECK(fling_controller);
  fling_controller_ = fling_controller;
  if (observed_begin_frame_source_) {
    return;
  }
  StartObservingBeginFrames();
}

void FlingSchedulerAndroid::DidStopFlingingOnBrowser(
    base::WeakPtr<input::FlingController> fling_controller) {
  DCHECK(fling_controller);
  StopObservingBeginFrames();
  fling_controller_ = nullptr;
  rir_->DidStopFlinging();
}

bool FlingSchedulerAndroid::NeedsBeginFrameForFlingProgress() {
  // Viz never receives an OnAnimate call since it don't have access to
  // WindowAndroid. Hence, it always fall back to BeginFrames notifications
  // coming from corresponding CompositorFrameSinkSupport's BeginFrameSource in
  // Viz.
  return true;
}

bool FlingSchedulerAndroid::ShouldUseMobileFlingCurve() {
  return true;
}

gfx::Vector2dF FlingSchedulerAndroid::GetPixelsPerInch(
    const gfx::PointF& position_in_screen) {
  return gfx::Vector2dF(input::kDefaultPixelsPerInch,
                        input::kDefaultPixelsPerInch);
}

void FlingSchedulerAndroid::ProgressFlingOnBeginFrameIfneeded(
    base::TimeTicks current_time) {
  // Called from synchronous compositor, which is used by WebView, which is not
  // covered by InputVizard currently.
  NOTREACHED();
}

BeginFrameSource* FlingSchedulerAndroid::GetBeginFrameSource() {
  return delegate_->GetBeginFrameSourceForFrameSink(frame_sink_id_);
}

void FlingSchedulerAndroid::StartObservingBeginFrames() {
  if (!fling_controller_) {
    return;
  }

  if (observed_begin_frame_source_) {
    return;
  }

  auto* begin_frame_provider = GetBeginFrameSource();
  if (!begin_frame_provider) {
    return;
  }

  begin_frame_provider->AddObserver(this);
  observed_begin_frame_source_ = begin_frame_provider;
}

void FlingSchedulerAndroid::StopObservingBeginFrames() {
  if (!observed_begin_frame_source_) {
    return;
  }
  observed_begin_frame_source_->RemoveObserver(this);
  observed_begin_frame_source_ = nullptr;
}

bool FlingSchedulerAndroid::OnBeginFrameDerivedImpl(
    const BeginFrameArgs& args) {
  DCHECK(observed_begin_frame_source_);
  if (!fling_controller_) {
    StopObservingBeginFrames();
    return false;
  }

  if (args.type == BeginFrameArgs::MISSED) {
    return false;
  }

  fling_controller_->ProgressFling(args.frame_time);
  return true;
}

bool FlingSchedulerAndroid::IsRoot() const {
  return false;
}

}  // namespace viz
