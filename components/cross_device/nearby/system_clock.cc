// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/nearby/src/internal/platform/implementation/system_clock.h"

#include "base/check_op.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/time/time.h"

#if BUILDFLAG(IS_MAC)
#include <mach/mach_time.h>
#include <time.h>
#elif BUILDFLAG(IS_POSIX)
#include <time.h>
#else
#include "base/system/sys_info.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_POSIX)

namespace nearby {

void SystemClock::Init() {}

absl::Time SystemClock::ElapsedRealtime() {
#if BUILDFLAG(IS_MAC)
  return absl::FromUnixMicros(
      base::TimeDelta::FromMachTime(mach_continuous_time()).InMicroseconds());
#elif BUILDFLAG(IS_POSIX)
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
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_POSIX)
}

Exception SystemClock::Sleep(absl::Duration duration) {
  base::PlatformThread::Sleep(
      base::Microseconds(absl::ToInt64Microseconds(duration)));
  return {Exception::kSuccess};
}

}  // namespace nearby
