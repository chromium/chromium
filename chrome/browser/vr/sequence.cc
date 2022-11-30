// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/vr/sequence.h"

namespace vr {

Sequence::SequencedTask::SequencedTask(base::OnceCallback<void()> task,
                                       base::TimeDelta delta)
    : task(std::move(task)), delta(delta) {}

Sequence::SequencedTask::SequencedTask(Sequence::SequencedTask&& other)
    : task(std::move(other.task)), delta(other.delta) {}

Sequence::SequencedTask::~SequencedTask() = default;

Sequence::Sequence() = default;
Sequence::~Sequence() = default;

void Sequence::Tick(base::TimeTicks now) {
  if (!started_) {
    started_ = true;
    start_time_ = now;
  }
  auto delta = now - start_time_;
  while (!empty() && tasks_.front().delta <= delta) {
    std::move(tasks_.front().task).Run();
    tasks_.pop_front();
  }
}

void Sequence::Add(base::OnceCallback<void()> task, base::TimeDelta delta) {
  DCHECK(empty() || tasks_.back().delta <= delta);
  SequencedTask sequenced_task(std::move(task), delta);
  tasks_.push_back(std::move(sequenced_task));
}

}  // namespace vr
