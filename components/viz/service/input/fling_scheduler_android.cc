// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/fling_scheduler_android.h"

#include "components/input/fling_controller.h"
#include "components/input/render_input_router.h"

namespace viz {

FlingSchedulerAndroid::FlingSchedulerAndroid(input::RenderInputRouter* rir,
                                             const FrameSinkId& frame_sink_id)
    : rir_(*rir), frame_sink_id_(frame_sink_id) {}

FlingSchedulerAndroid::~FlingSchedulerAndroid() {
  StopObservingBeginFrames();
}

void FlingSchedulerAndroid::ScheduleFlingProgress(
    base::WeakPtr<input::FlingController> fling_controller) {
  DCHECK(fling_controller);
  fling_controller_ = fling_controller;
  if (observing_begin_frame_source_) {
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

bool FlingSchedulerAndroid::ProgressFlingOnFlingStart() {
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

void FlingSchedulerAndroid::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  if (!begin_frame_source) {
    StopObservingBeginFrames();
  }
  CHECK(!observing_begin_frame_source_);
  begin_frame_source_ = begin_frame_source;
}

BeginFrameSource* FlingSchedulerAndroid::GetBeginFrameSource() {
  return begin_frame_source_;
}

void FlingSchedulerAndroid::StartObservingBeginFrames() {
  if (!fling_controller_) {
    return;
  }

  if (observing_begin_frame_source_) {
    return;
  }

  if (GetBeginFrameSource()) {
    observing_begin_frame_source_ = true;
    GetBeginFrameSource()->AddObserver(this);
  }
}

void FlingSchedulerAndroid::StopObservingBeginFrames() {
  if (!observing_begin_frame_source_) {
    return;
  }
  GetBeginFrameSource()->RemoveObserver(this);
  observing_begin_frame_source_ = false;
}

bool FlingSchedulerAndroid::OnBeginFrameDerivedImpl(
    const BeginFrameArgs& args) {
  DCHECK(observing_begin_frame_source_);
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

}  // namespace viz
