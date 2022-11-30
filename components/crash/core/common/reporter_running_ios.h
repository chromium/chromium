// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_REPORTER_RUNNING_IOS_H_
#define COMPONENTS_CRASH_CORE_COMMON_REPORTER_RUNNING_IOS_H_

namespace crash_reporter {

// Returns true if Breakpad is installed and running.
bool IsBreakpadRunning();

// Sets whether Breakpad is installed and running.
void SetBreakpadRunning(bool running);

// Returns true if Crashpad is installed and running.
bool IsCrashpadRunning();

// Sets whether Crashpad is installed and running.
void SetCrashpadRunning(bool running);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_COMMON_REPORTER_RUNNING_IOS_H_
