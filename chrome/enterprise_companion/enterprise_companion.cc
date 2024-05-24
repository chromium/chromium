// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "chrome/enterprise_companion/lock.h"

namespace {

constexpr char kLoggingModuleSwitch[] = "vmodule";
constexpr char kLoggingModuleSwitchValue[] =
    "*/chrome/enterprise_companion/*=2";

void InitLogging() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kLoggingModuleSwitch)) {
    command_line->AppendSwitchASCII(kLoggingModuleSwitch,
                                    kLoggingModuleSwitchValue);
  }
  logging::InitLogging({.logging_dest = logging::LOG_TO_STDERR});
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);
}

}  // namespace

namespace enterprise_companion {

int EnterpriseCompanionMain(int argc, const char* const* argv) {
  base::PlatformThread::SetName("EnterpriseCompanion");
  base::CommandLine::Init(argc, argv);
  InitLogging();

  std::unique_ptr<ScopedLock> lock = CreateScopedLock();
  if (!lock) {
    LOG(ERROR) << "Failed to acquire singleton lock. Exiting.";
    return 1;
  }

  VLOG(1) << "Launching Chrome Enterprise Companion";
  return 0;
}

}  // namespace enterprise_companion
