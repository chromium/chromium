// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "chrome/enterprise_companion/enterprise_companion_service.h"
#include "chrome/enterprise_companion/enterprise_companion_service_stub.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/lock.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"

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

void InitThreadPool() {
  base::PlatformThread::SetName("EnterpriseCompanion");
  base::ThreadPoolInstance::Create("EnterpriseCompanion");

  // Reuses the logic in base::ThreadPoolInstance::StartWithDefaultParams.
  const size_t max_num_foreground_threads =
      static_cast<size_t>(std::max(3, base::SysInfo::NumberOfProcessors() - 1));
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
  base::ThreadPoolInstance::Get()->Start(init_params);
}

}  // namespace

namespace enterprise_companion {

int EnterpriseCompanionMain(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  InitLogging();
  InitThreadPool();

  base::SingleThreadTaskExecutor main_task_executor;
  ScopedIPCSupportWrapper ipc_support;

  std::unique_ptr<ScopedLock> lock = CreateScopedLock();
  if (!lock) {
    LOG(ERROR) << "Failed to acquire singleton lock. Exiting.";
    return 1;
  }

  VLOG(1) << "Launching Chrome Enterprise Companion";
  base::RunLoop run_loop;
  std::unique_ptr<mojom::EnterpriseCompanion> stub =
      CreateEnterpriseCompanionServiceStub(
          CreateEnterpriseCompanionService(run_loop.QuitClosure()));
  run_loop.Run();

  return 0;
}

}  // namespace enterprise_companion
