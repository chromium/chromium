// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/shutdown_watchdog.h"

#include <utility>

#include "base/logging.h"
#include "chrome/chrome_cleaner/os/early_exit.h"

namespace chrome_cleaner {

ShutdownWatchdog::ShutdownWatchdog(const base::TimeDelta& duration,
                                   AlarmCallback callback)
    : watchdog_(duration, "Shutdown watchdog", true, this) {
  callback_ = std::move(callback);
}

ShutdownWatchdog::~ShutdownWatchdog() = default;

void ShutdownWatchdog::Arm() {
  watchdog_.Arm();
}
void ShutdownWatchdog::Disarm() {
  watchdog_.Disarm();
}

void ShutdownWatchdog::Alarm() {
  int exit_code = std::move(callback_).Run();
  LOG(ERROR) << "Shutdown watchdog triggered, exiting with code " << exit_code;
  EarlyExit(exit_code);
}

}  // namespace chrome_cleaner
