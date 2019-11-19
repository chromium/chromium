// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_CRASH_SWITCHES_H_
#define COMPONENTS_CRASH_CONTENT_APP_CRASH_SWITCHES_H_

#include "build/build_config.h"

namespace crash_reporter {
namespace switches {

extern const char kCrashpadHandler[];

#if defined(OS_LINUX)
extern const char kCrashpadHandlerPid[];
#endif  // OS_LINUX

#if defined(OS_CHROMEOS)
extern const char kCrashLoopBefore[];
#endif

}  // namespace switches
}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_APP_CRASH_SWITCHES_H_
