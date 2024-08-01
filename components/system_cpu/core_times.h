// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_SYSTEM_CPU_CORE_TIMES_H_
#define COMPONENTS_SYSTEM_CPU_CORE_TIMES_H_

#include <stdint.h>

#include <initializer_list>

#include "base/gtest_prod_util.h"

namespace system_cpu {

// CPU core utilization statistics.
//
// Linux:
// Quantities are expressed in "user hertz", which is a Linux kernel
// configuration knob (USER_HZ). Typical values range between 1/100 seconds
// and 1/1000 seconds. The denominator can be obtained from
// sysconf(_SC_CLK_TCK).
//
// Mac:
// Quantities are expressed in "CPU Ticks", which is an arbitrary unit of time
// recording how many intervals of time elapsed, typically 1/100 of a second.
class CoreTimes {
 public:
  CoreTimes() = default;
  ~CoreTimes() = default;

  // Normal processes executing in user mode.
  uint64_t user() const { return times_[0]; }
  // Niced processes executing in user mode.
  uint64_t nice() const { return times_[1]; }
  // Processes executing in kernel mode.
  uint64_t system() const { return times_[2]; }
  // Twiddling thumbs.
  uint64_t idle() const { return times_[3]; }
  // Waiting for I/O to complete. Unreliable.
  uint64_t iowait() const { return times_[4]; }
  // Servicing interrupts.
  uint64_t irq() const { return times_[5]; }
  // Servicing softirqs.
  uint64_t softirq() const { return times_[6]; }
  // Involuntary wait.
  uint64_t steal() const { return times_[7]; }
  // Running a normal guest.
  uint64_t guest() const { return times_[8]; }
  // Running a niced guest.
  uint64_t guest_nice() const { return times_[9]; }

  // Setters.
  //
  // Ensure that the reported core usage times are monotonically increasing.
  // We assume that by any decrease is a temporary blip.
  void set_user(uint64_t time);
  void set_nice(uint64_t time);
  void set_system(uint64_t time);
  void set_idle(uint64_t time);
  void set_iowait(uint64_t time);
  void set_irq(uint64_t time);
  void set_softirq(uint64_t time);
  void set_steal(uint64_t time);
  void set_guest(uint64_t time);
  void set_guest_nice(uint64_t time);

  // Computes a CPU's utilization over the time between two stat snapshots.
  //
  // Returns a value between 0.0 and 1.0 on success, and -1.0 when given
  // invalid data, such as a `baseline` that does not represent a stat
  // snapshot collected before `this` snapshot.
  double TimeUtilization(const CoreTimes& baseline) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CoreTimesTest, TimeUtilizationBalanced);
  FRIEND_TEST_ALL_PREFIXES(CoreTimesTest, TimeUtilizationEmptyRange);
  FRIEND_TEST_ALL_PREFIXES(CoreTimesTest, TimeUtilizationNegativeRange);
  FRIEND_TEST_ALL_PREFIXES(CoreTimesTest, TimeUtilizationComputation);

  // Used by CoreTimesTest.
  CoreTimes(const std::initializer_list<uint64_t>& times);

  uint64_t times_[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

}  // namespace system_cpu

#endif  // COMPONENTS_SYSTEM_CPU_CORE_TIMES_H_
