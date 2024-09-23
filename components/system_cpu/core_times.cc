// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/system_cpu/core_times.h"

#include "base/check_op.h"

namespace system_cpu {

CoreTimes::CoreTimes(const std::initializer_list<uint64_t>& times) {
  CHECK_EQ(times.size(), 10u);

  size_t i = 0;
  for (auto value : times) {
    times_[i++] = value;
  }
}

void CoreTimes::set_user(uint64_t time) {
  if (times_[0] < time) {
    times_[0] = time;
  }
}

void CoreTimes::set_nice(uint64_t time) {
  if (times_[1] < time) {
    times_[1] = time;
  }
}

void CoreTimes::set_system(uint64_t time) {
  if (times_[2] < time) {
    times_[2] = time;
  }
}

void CoreTimes::set_idle(uint64_t time) {
  if (times_[3] < time) {
    times_[3] = time;
  }
}

void CoreTimes::set_iowait(uint64_t time) {
  if (times_[4] < time) {
    times_[4] = time;
  }
}

void CoreTimes::set_irq(uint64_t time) {
  if (times_[5] < time) {
    times_[5] = time;
  }
}

void CoreTimes::set_softirq(uint64_t time) {
  if (times_[6] < time) {
    times_[6] = time;
  }
}

void CoreTimes::set_steal(uint64_t time) {
  if (times_[7] < time) {
    times_[7] = time;
  }
}

void CoreTimes::set_guest(uint64_t time) {
  if (times_[8] < time) {
    times_[8] = time;
  }
}

void CoreTimes::set_guest_nice(uint64_t time) {
  if (times_[9] < time) {
    times_[9] = time;
  }
}

double CoreTimes::TimeUtilization(const CoreTimes& baseline) const {
  // Each of the blocks below consists of a check and a subtraction. The check
  // is used to bail on invalid input (/proc/stat counters should never
  // decrease over time).
  //
  // The check is also essential for the correctness of the subtraction -- the
  // result of the subtraction is stored in a temporary `uint64_t` before being
  // accumulated in `active_delta`, and this intermediate result must not be
  // negative.

  if (user() < baseline.user()) {
    return -1;
  }
  double active_delta = user() - baseline.user();

  if (nice() < baseline.nice()) {
    return -1;
  }
  active_delta += nice() - baseline.nice();

  if (system() < baseline.system()) {
    return -1;
  }
  active_delta += system() - baseline.system();

  if (idle() < baseline.idle()) {
    return -1;
  }
  uint64_t idle_delta = idle() - baseline.idle();

  // iowait() is unreliable, according to the Linux kernel documentation at
  // https://www.kernel.org/doc/Documentation/filesystems/proc.txt.

  if (irq() < baseline.irq()) {
    return -1;
  }
  active_delta += irq() - baseline.irq();

  if (softirq() < baseline.softirq()) {
    return -1;
  }
  active_delta += softirq() - baseline.softirq();

  if (steal() < baseline.steal()) {
    return -1;
  }
  active_delta += steal() - baseline.steal();

  // guest() and guest_nice() are included in user(). Full analysis in
  // https://unix.stackexchange.com/a/303224/

  double total_delta = active_delta + idle_delta;
  if (total_delta == 0) {
    // The two snapshots represent the same point in time, so the time interval
    // between the two snapshots is empty.
    return -1;
  }

  return active_delta / total_delta;
}

}  // namespace system_cpu
