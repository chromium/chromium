// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion.h"

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_bound.h"
#include "chrome/enterprise_companion/dm_client.h"
#include "chrome/enterprise_companion/enterprise_companion_service.h"
#include "chrome/enterprise_companion/enterprise_companion_service_stub.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/lock.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom-forward.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "chrome/enterprise_companion/url_loader_factory_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_companion {

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

class EnterpriseCompanionApp {
 public:
  EnterpriseCompanionApp() {
    net_thread_.StartWithOptions({base::MessagePumpType::IO, 0});
    url_loader_factory_provider_ =
        base::SequenceBound<URLLoaderFactoryProvider>(
            net_thread_.task_runner());
  }

  ~EnterpriseCompanionApp() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void RunUntilShutdown() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::RunLoop run_loop;
    url_loader_factory_provider_
        .AsyncCall(&URLLoaderFactoryProvider::GetPendingURLLoaderFactory)
        .Then(base::BindOnce(
            &EnterpriseCompanionApp::OnUrlLoaderFactoryReceived,
            weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));
    run_loop.Run();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::Thread net_thread_{"Network"};
  base::SequenceBound<URLLoaderFactoryProvider> url_loader_factory_provider_;
  std::unique_ptr<mojom::EnterpriseCompanion> stub_;

  void OnUrlLoaderFactoryReceived(
      base::OnceClosure shutdown_callback,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        network::SharedURLLoaderFactory::Create(
            std::move(pending_url_loader_factory));

    VLOG(1) << "Launching Chrome Enterprise Companion";
    stub_ =
        CreateEnterpriseCompanionServiceStub(CreateEnterpriseCompanionService(
            CreateDMClient(
                GetDefaultCloudPolicyClientProvider(url_loader_factory)),
            std::move(shutdown_callback)));
  }

  base::WeakPtrFactory<EnterpriseCompanionApp> weak_ptr_factory_{this};
};

}  // namespace

int EnterpriseCompanionMain(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  InitLogging();
  InitThreadPool();
  base::AtExitManager exit_manager;

  base::SingleThreadTaskExecutor main_task_executor;
  ScopedIPCSupportWrapper ipc_support;

  std::unique_ptr<ScopedLock> lock = CreateScopedLock();
  if (!lock) {
    LOG(ERROR) << "Failed to acquire singleton lock. Exiting.";
    return 1;
  }

  EnterpriseCompanionApp().RunUntilShutdown();
  return 0;
}

}  // namespace enterprise_companion
