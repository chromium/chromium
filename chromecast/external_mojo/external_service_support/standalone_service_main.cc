// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "chromecast/external_mojo/external_service_support/process_setup.h"
#include "chromecast/external_mojo/external_service_support/service_process.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

// Simple process entrypoint for standalone Mojo services.

struct GlobalState {
  std::unique_ptr<chromecast::external_service_support::ServiceProcess>
      service_process;
  std::unique_ptr<chromecast::external_service_support::ExternalConnector>
      connector;
};

void OnConnected(
    GlobalState* state,
    std::unique_ptr<chromecast::external_service_support::ExternalConnector>
        connector) {
  state->connector = std::move(connector);
  state->service_process =
      chromecast::external_service_support::ServiceProcess::Create(
          state->connector.get());
}

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  chromecast::external_service_support::CommonProcessInitialization(argc, argv);

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;

  mojo::core::Init();

  mojo::core::ScopedIPCSupport ipc_support(
      io_task_executor.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "StandaloneService");

  GlobalState state;
  chromecast::external_service_support::ExternalConnector::Connect(
      chromecast::external_mojo::GetBrokerPath(),
      base::BindOnce(&OnConnected, &state));

  run_loop.Run();
  base::ThreadPoolInstance::Get()->Shutdown();

  return 0;
}
