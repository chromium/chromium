// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/crash/core/common/reporter_running_ios.h"

namespace crash_reporter {

namespace {

bool g_breakpad_running;
bool g_crashpad_running;

}  // namespace

bool IsBreakpadRunning() {
  return g_breakpad_running;
}

void SetBreakpadRunning(bool running) {
  g_breakpad_running = running;
}

bool IsCrashpadRunning() {
  return g_crashpad_running;
}

void SetCrashpadRunning(bool running) {
  g_crashpad_running = running;
}

}  // namespace crash_reporter
