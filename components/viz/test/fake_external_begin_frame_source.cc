// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_external_begin_frame_source.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/viz/test/begin_frame_args_test.h"

namespace viz {

FakeExternalBeginFrameSource::FakeExternalBeginFrameSource(
    double refresh_rate,
    bool tick_automatically)
    : BeginFrameSource(kNotRestartableId),
      tick_automatically_(tick_automatically),
      milliseconds_per_frame_(1000.0 / refresh_rate) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FakeExternalBeginFrameSource::~FakeExternalBeginFrameSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeExternalBeginFrameSource::SetPaused(bool paused) {
  if (paused != paused_) {
    paused_ = paused;
    std::set<BeginFrameObserver*> observers(observers_);
    for (auto* obs : observers)
      obs->OnBeginFrameSourcePausedChanged(paused_);
  }
}

void FakeExternalBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  bool observers_was_empty = observers_.empty();
  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(paused_);
  if (observers_was_empty && tick_automatically_) {
    PostTestOnBeginFrame();
  } else if (current_args_.IsValid()) {
    const BeginFrameArgs& last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        last_args.frame_time < current_args_.frame_time) {
      current_args_.type = BeginFrameArgs::MISSED;
      obs->OnBeginFrame(current_args_);
    }
  }
  if (client_)
    client_->OnAddObserver(obs);
}

void FakeExternalBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());

  observers_.erase(obs);
  if (observers_.empty())
    begin_frame_task_.Cancel();
  if (client_)
    client_->OnRemoveObserver(obs);
}

void FakeExternalBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs) {}

bool FakeExternalBeginFrameSource::IsThrottled() const {
  return true;
}

BeginFrameArgs FakeExternalBeginFrameSource::CreateBeginFrameArgs(
    BeginFrameArgs::CreationLocation location,
    const base::TickClock* now_src) {
  return CreateBeginFrameArgsForTesting(location, source_id(),
                                        next_begin_frame_number_++, now_src);
}

BeginFrameArgs FakeExternalBeginFrameSource::CreateBeginFrameArgs(
    BeginFrameArgs::CreationLocation location) {
  return CreateBeginFrameArgsForTesting(location, source_id(),
                                        next_begin_frame_number_++);
}

void FakeExternalBeginFrameSource::TestOnBeginFrame(
    const BeginFrameArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_args_ = args;
  std::set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers)
    obs->OnBeginFrame(current_args_);
  if (tick_automatically_)
    PostTestOnBeginFrame();
}

void FakeExternalBeginFrameSource::PostTestOnBeginFrame() {
  begin_frame_task_.Reset(
      base::BindOnce(&FakeExternalBeginFrameSource::TestOnBeginFrame,
                     weak_ptr_factory_.GetWeakPtr(),
                     CreateBeginFrameArgs(BEGINFRAME_FROM_HERE)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, begin_frame_task_.callback(),
      base::TimeDelta::FromMilliseconds(milliseconds_per_frame_));
  next_begin_frame_number_++;
}

}  // namespace viz
