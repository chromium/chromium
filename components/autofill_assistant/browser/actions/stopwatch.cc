// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/stopwatch.h"

#include <ostream>
#include <string>

#include "base/logging.h"

namespace autofill_assistant {

Stopwatch::Stopwatch() = default;

bool Stopwatch::Start() {
  if (running_) {
    return false;
  }
  running_ = true;
  start_time_ = base::TimeTicks::Now();
  return true;
}

void Stopwatch::StartAt(base::TimeTicks start_time) {
  running_ = true;
  start_time_ = start_time;
}

bool Stopwatch::Stop() {
  if (!running_) {
    return false;
  }
  elapsed_time_ += LastElapsedAt(base::TimeTicks::Now());
  running_ = false;
  return true;
}

bool Stopwatch::StopAt(base::TimeTicks stop_time) {
  if (!running_) {
    return false;
  }
  elapsed_time_ += LastElapsedAt(stop_time);
  running_ = false;
  return true;
}

void Stopwatch::AddTime(base::TimeDelta time) {
  elapsed_time_ += time;
}

void Stopwatch::AddTime(const Stopwatch& other) {
  AddTime(other.TotalElapsed());
}

void Stopwatch::RemoveTime(base::TimeDelta time) {
  if (elapsed_time_ > time) {
    elapsed_time_ -= time;
  } else {
    elapsed_time_ = base::Milliseconds(0);
    if (running_) {
      start_time_ += (time - elapsed_time_);
    }
  }
}

void Stopwatch::RemoveTime(const Stopwatch& other) {
  RemoveTime(other.TotalElapsed());
}

void Stopwatch::Reset() {
  Stop();
  elapsed_time_ = base::Milliseconds(0);
}

base::TimeDelta Stopwatch::TotalElapsed() const {
  return elapsed_time_ + LastElapsedAt(base::TimeTicks::Now());
}

base::TimeDelta Stopwatch::LastElapsedAt(base::TimeTicks time) const {
  if (!running_) {
    return base::Milliseconds(0);
  }
  return time > start_time_ ? time - start_time_ : base::Milliseconds(0);
}

bool Stopwatch::IsRunning() const {
  return running_;
}

std::ostream& operator<<(std::ostream& out, const Stopwatch& stopwatch) {
  out << (stopwatch.elapsed_time_ + stopwatch.LastElapsedAt(base::TimeTicks()))
             .InMilliseconds()
      << " (currently " << (stopwatch.running_ ? "" : "not ") << "running)";
  return out;
}

void ActionStopwatch::TransferToActiveTime(base::TimeDelta time) {
  active_time_stopwatch_.AddTime(time);
  wait_time_stopwatch_.RemoveTime(time);
}

void ActionStopwatch::TransferToWaitTime(base::TimeDelta time) {
  wait_time_stopwatch_.AddTime(time);
  active_time_stopwatch_.RemoveTime(time);
}

void ActionStopwatch::StartActiveTime() {
  wait_time_stopwatch_.Stop();
  active_time_stopwatch_.Start();
}
void ActionStopwatch::StartActiveTimeAt(base::TimeTicks start_time) {
  wait_time_stopwatch_.StopAt(start_time);
  active_time_stopwatch_.StartAt(start_time);
}
void ActionStopwatch::StartWaitTime() {
  active_time_stopwatch_.Stop();
  wait_time_stopwatch_.Start();
}

void ActionStopwatch::StartWaitTimeAt(base::TimeTicks start_time) {
  active_time_stopwatch_.StopAt(start_time);
  wait_time_stopwatch_.StartAt(start_time);
}

void ActionStopwatch::Stop() {
  active_time_stopwatch_.Stop();
  wait_time_stopwatch_.Stop();
}

base::TimeDelta ActionStopwatch::TotalActiveTime() {
  return active_time_stopwatch_.TotalElapsed();
}

base::TimeDelta ActionStopwatch::TotalWaitTime() {
  return wait_time_stopwatch_.TotalElapsed();
}

std::ostream& operator<<(std::ostream& out,
                         const ActionStopwatch& action_stopwatch) {
  out << "Active time: " << action_stopwatch.active_time_stopwatch_
      << " Wait time: " << action_stopwatch.wait_time_stopwatch_;
  return out;
}

}  // namespace autofill_assistant
