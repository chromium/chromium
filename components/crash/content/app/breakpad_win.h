// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_WIN_H_
#define COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_WIN_H_

#include <windows.h>
#include <string>

namespace breakpad {

void InitCrashReporter(const std::string& process_type_switch);

// If chrome has been restarted because it crashed, this function will display
// a dialog asking for permission to continue execution or to exit now.
bool ShowRestartDialogIfCrashed(bool* exit_now);

// Tells Breakpad that our process is shutting down and to consume
// EXCEPTION_INVALID_HANDLE exceptions which occur if bad handle detection is
// enabled and the sandbox handle closer has previously closed handles owned by
// Windows DLLs.
void ConsumeInvalidHandleExceptions();

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_WIN_H_
