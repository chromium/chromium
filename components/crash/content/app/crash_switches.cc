// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crash_switches.h"

namespace crash_reporter {
namespace switches {

// A process type (switches::kProcessType) that indicates chrome.exe or
// setup.exe is being launched as crashpad_handler. This is only used on
// Windows. We bundle the handler into chrome.exe on Windows because there is
// high probability of a "new" .exe being blocked or interfered with by
// application firewalls, AV software, etc. On other platforms, crashpad_handler
// is a standalone executable.
const char kCrashpadHandler[] = "crashpad-handler";

#if defined(OS_LINUX)
// The process ID of the Crashpad handler.
const char kCrashpadHandlerPid[] = "crashpad-handler-pid";
#endif

#if defined(OS_CHROMEOS)
// A time_t. Passed by session_manager into the Chrome user session, indicating
// that if Chrome crashes before the indicated time, session_manager will
// consider this to be a crash-loop situation and log the user out. Chrome
// mostly just passes this to crash_reporter if it crashes.
const char kCrashLoopBefore[] = "crash-loop-before";
#endif

}  // namespace switches
}  // namespace crash_reporter
