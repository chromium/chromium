// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/nearby/src/cpp/platform/api/system_clock.h"

#include "base/check_op.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/time/time.h"

#if defined(OS_MAC)
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "base/stl_util.h"
#elif defined(OS_POSIX)
#include <time.h>
#else
#include "base/system/sys_info.h"
#endif  // defined(OS_MAC) || defined(OS_POSIX)

namespace location {
namespace nearby {

void SystemClock::Init() {}

absl::Time SystemClock::ElapsedRealtime() {
#if defined(OS_MAC)
  // Mac 10.12 supports mach_continuous_time, which is around 15 times faster
  // than sysctl() call. Use it if possible; otherwise, fall back to sysctl().
  if (__builtin_available(macOS 10.12, *)) {
    return absl::FromUnixMicros(
        base::TimeDelta::FromMachTime(mach_continuous_time()).InMicroseconds());
  }

  // On Mac mach_absolute_time stops while the device is sleeping. Instead use
  // now - KERN_BOOTTIME to get a time difference that is not impacted by clock
  // changes. KERN_BOOTTIME will be updated by the system whenever the system
  // clock change.
  struct timeval boottime;
  int mib[2] = {CTL_KERN, KERN_BOOTTIME};
  size_t size = sizeof(boottime);
  int kr = sysctl(mib, base::size(mib), &boottime, &size, nullptr, 0);
  DCHECK_EQ(KERN_SUCCESS, kr);
  base::TimeDelta time_difference =
      base::Time::FromCFAbsoluteTime(CFAbsoluteTimeGetCurrent()) -
      (base::Time::FromTimeT(boottime.tv_sec) +
       base::TimeDelta::FromMicroseconds(boottime.tv_usec));
  return absl::FromUnixMicros(time_difference.InMicroseconds());
#elif defined(OS_POSIX)
  // SystemClock::ElapsedRealtime() must provide monotonically increasing time,
  // but is not expected to be convertible to wall clock time. Unfortunately,
  // the POSIX implementation of base::SysInfo::Uptime(), i.e. TimeTicks::Now(),
  // does not increase when system is suspended. We instead use clock_gettime
  // directly. See https://crbug.com/166153.
  struct timespec ts = {};
  const int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DCHECK_EQ(ret, 0);
  return absl::TimeFromTimespec(ts);
#else
  return absl::FromUnixMicros(base::SysInfo::Uptime().InMicroseconds());
#endif  // defined(OS_MAC) || defined(OS_POSIX)
}

Exception SystemClock::Sleep(absl::Duration duration) {
  base::PlatformThread::Sleep(
      base::TimeDelta::FromMicroseconds(absl::ToInt64Microseconds(duration)));
  return {Exception::kSuccess};
}

}  // namespace nearby
}  // namespace location
