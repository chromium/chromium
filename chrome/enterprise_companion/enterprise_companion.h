// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_H_

#include <optional>

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace enterprise_companion {

// Specifies the logging module filter.
extern const char kLoggingModuleSwitch[];

// The default logging module switch value.
extern const char kLoggingModuleSwitchValue[];

// Runs as the embedded Crashpad handler.
extern const char kCrashHandlerSwitch[];

// Crash the program for testing purposes.
extern const char kCrashMeSwitch[];

// Install the application.
extern const char kInstallSwitch[];

// Remove all traces of the application from the system.
extern const char kUninstallSwitch[];

#if BUILDFLAG(IS_MAC)
// Runs the network worker.
extern const char kNetWorkerSwitch[];
#endif

int EnterpriseCompanionMain(int argc, const char* const* argv);

std::optional<base::FilePath> GetLogFilePath();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_H_
