// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/crashpad_handler.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"

std::optional<int> RunAsCrashpadHandlerIfRequired(
    const base::CommandLine& cmd_line) {
  if (cmd_line.GetSwitchValueASCII(switches::kProcessType) ==
      crash_reporter::switches::kCrashpadHandler) {
    return crash_reporter::RunAsCrashpadHandler(cmd_line, base::FilePath(),
                                                switches::kProcessType,
                                                switches::kUserDataDir);
  }
  return std::nullopt;
}
