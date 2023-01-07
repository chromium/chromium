// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_PROCESS_EXIT_REASON_FROM_SYSTEM_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_PROCESS_EXIT_REASON_FROM_SYSTEM_ANDROID_H_

#include <string>

#include "base/process/process_handle.h"

namespace crash_reporter {

// Wrapper around process exit reason from ActivityManager.
class ProcessExitReasonFromSystem {
 public:
  // Gets the exit reason of processes from ActivityManager and records enum
  // histogram to UMA with |uma_name|.
  static void RecordExitReasonToUma(base::ProcessId pid,
                                    const std::string& uma_name);
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_PROCESS_EXIT_REASON_FROM_SYSTEM_ANDROID_H_
