// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_CRASHPAD_HANDLER_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_CRASHPAD_HANDLER_H_

#include <optional>

namespace base {
class CommandLine;
}

// Runs the current process as a crashpad handler if built with crashpad support
// and `command_line` contains --type=crashpad-handler. Returns the process
// exit code if the handler was run, or std::nullopt otherwise.
std::optional<int> RunAsCrashpadHandlerIfRequired(
    const base::CommandLine& command_line);

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_CRASHPAD_HANDLER_H_
