// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/primary_begin_frame_source.h"

namespace viz {

PrimaryBeginFrameSource::PrimaryBeginFrameSource()
    : BeginFrameSource(kNotRestartableId), begin_frame_source_(this) {}

PrimaryBeginFrameSource::~PrimaryBeginFrameSource() = default;

void PrimaryBeginFrameSource::OnBeginFrameSourceAdded(
    BeginFrameSource* begin_frame_source) {
  sources_.insert(begin_frame_source);

  if (current_begin_frame_source_)
    return;

  current_begin_frame_source_ = begin_frame_source;
  if (needs_begin_frames_ && current_begin_frame_source_)
    current_begin_frame_source_->AddObserver(this);
}

void PrimaryBeginFrameSource::OnBeginFrameSourceRemoved(
    BeginFrameSource* begin_frame_source) {
  sources_.erase(begin_frame_source);
  if (current_begin_frame_source_ != begin_frame_source)
    return;

  if (needs_begin_frames_)
    current_begin_frame_source_->RemoveObserver(this);

  if (!sources_.empty())
    current_begin_frame_source_ = *sources_.begin();
  else
    current_begin_frame_source_ = nullptr;

  if (needs_begin_frames_ && current_begin_frame_source_)
    current_begin_frame_source_->AddObserver(this);
}

void PrimaryBeginFrameSource::OnBeginFrame(const BeginFrameArgs& args) {
  begin_frame_source_.OnBeginFrame(args);
  last_begin_frame_args_ = args;
}

const BeginFrameArgs& PrimaryBeginFrameSource::LastUsedBeginFrameArgs() const {
  return last_begin_frame_args_;
}

bool PrimaryBeginFrameSource::WantsAnimateOnlyBeginFrames() const {
  // Forward animate_only BeginFrames.
  return true;
}

void PrimaryBeginFrameSource::OnBeginFrameSourcePausedChanged(bool paused) {}

void PrimaryBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs) {
  begin_frame_source_.DidFinishFrame(obs);
}

void PrimaryBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  begin_frame_source_.AddObserver(obs);
}

void PrimaryBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  begin_frame_source_.RemoveObserver(obs);
}

bool PrimaryBeginFrameSource::IsThrottled() const {
  return current_begin_frame_source_
             ? current_begin_frame_source_->IsThrottled()
             : true;
}

void PrimaryBeginFrameSource::OnGpuNoLongerBusy() {
  // PrimaryBeginFrameSource does not hold back the begin frames. So it doesn't
  // need to do anything here.
}

void PrimaryBeginFrameSource::OnNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames)
    return;

  needs_begin_frames_ = needs_begin_frames;

  if (!current_begin_frame_source_)
    return;

  if (needs_begin_frames_)
    current_begin_frame_source_->AddObserver(this);
  else
    current_begin_frame_source_->RemoveObserver(this);
}

}  // namespace viz
