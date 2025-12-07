// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_FLAGS_H_
#define CHROME_ENTERPRISE_COMPANION_FLAGS_H_

#include "build/build_config.h"

namespace enterprise_companion {

// Command-line flags indicating which app to run.
inline constexpr char kCrashHandlerSwitch[] = "crash-handler";
inline constexpr char kCrashMeSwitch[] = "crash-me";
inline constexpr char kShutdownSwitch[] = "shutdown";
inline constexpr char kFetchPoliciesSwitch[] = "fetch-policies";
inline constexpr char kInstallSwitch[] = "install";
inline constexpr char kUninstallSwitch[] = "uninstall";
#if BUILDFLAG(IS_MAC)
inline constexpr char kNetWorkerSwitch[] = "net-worker";
#endif

// Controls whether the transmission of usage stats is allowed.
inline constexpr char kEnableUsageStatsSwitch[] = "enable-usage-stats";

// Controls the Omaha cohort ID used in telemetry.
inline constexpr char kCohortIdSwitch[] = "cohort-id";

// Controls the logging configuration for the process.
inline constexpr char kLoggingModuleSwitch[] = "vmodule";

// Default logging module.
inline constexpr char kLoggingModuleSwitchValue[] =
    "*/chrome/enterprise_companion/*=2,*/components/*=2";

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_FLAGS_H_
