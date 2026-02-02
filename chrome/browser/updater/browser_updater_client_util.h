// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_

#include <cstdint>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/updater/updater_scope.h"

namespace base {
enum class TaskPriority : uint8_t;
}

namespace updater {

extern const char kUpdaterName[];
extern const char kPrivilegedHelperName[];

#if !BUILDFLAG(IS_LINUX)

// System level updater should only be used if the browser is owned by root.
// During promotion, the browser will be changed to be owned by root and wheel.
// A browser must go through promotion before it can utilize the system-level
// updater.
UpdaterScope GetBrowserUpdaterScope();

#endif  // !BUILDFLAG(IS_LINUX)

}  // namespace updater

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
