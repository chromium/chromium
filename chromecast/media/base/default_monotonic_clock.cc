// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/default_monotonic_clock.h"

#include <time.h>

#include <memory>

#include "base/time/time.h"
#include "build/build_config.h"

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "chromecast/media/base/buildflags.h"
#endif  // defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_FUCHSIA)
#include <zircon/syscalls.h>
#endif  // defined(OS_FUCHSIA)

namespace chromecast {
namespace media {

// static
std::unique_ptr<MonotonicClock> MonotonicClock::Create() {
  return std::make_unique<DefaultMonotonicClock>();
}

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
int64_t MonotonicClockNow() {
  timespec now = {0, 0};
#if BUILDFLAG(MEDIA_CLOCK_MONOTONIC_RAW)
  clock_gettime(CLOCK_MONOTONIC_RAW, &now);
#else
  clock_gettime(CLOCK_MONOTONIC, &now);
#endif  // MEDIA_CLOCK_MONOTONIC_RAW
  return base::TimeDelta::FromTimeSpec(now).InMicroseconds();
}

#elif defined(OS_FUCHSIA)
int64_t MonotonicClockNow() {
  return zx_clock_get_monotonic() / 1000;
}

#endif

DefaultMonotonicClock::DefaultMonotonicClock() = default;

DefaultMonotonicClock::~DefaultMonotonicClock() = default;

int64_t DefaultMonotonicClock::Now() const {
  return MonotonicClockNow();
}

}  // namespace media
}  // namespace chromecast
