// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "chromecast/external_mojo/external_service_support/process_setup.h"
#include "chromecast/external_mojo/external_service_support/service_process.h"
#include "chromecast/external_mojo/external_service_support/tracing_client.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "chromecast/external_mojo/public/cpp/external_mojo_broker.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

// Simple process entrypoint for standalone Mojo services.

struct GlobalState {
  std::unique_ptr<chromecast::external_service_support::ServiceProcess>
      service_process;
  std::unique_ptr<chromecast::external_service_support::ExternalConnector>
      connector;
  std::unique_ptr<chromecast::external_service_support::TracingClient>
      tracing_client;
};

void OnConnected(
    GlobalState* state,
    std::unique_ptr<chromecast::external_service_support::ExternalConnector>
        connector) {
  state->connector = std::move(connector);
  state->tracing_client =
      chromecast::external_service_support::TracingClient::Create(
          state->connector.get());
  state->service_process =
      chromecast::external_service_support::ServiceProcess::Create(
          state->connector.get());
}

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  chromecast::external_service_support::CommonProcessInitialization(argc, argv);

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::FileDescriptorWatcher file_descriptor_watcher(io_task_executor.task_runner());
  base::RunLoop run_loop;

  mojo::core::Init();

  mojo::core::ScopedIPCSupport ipc_support(
      io_task_executor.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "StandaloneService");

  GlobalState state;
  // State for in-process Mojo broker.
  auto broker_thread = std::make_unique<base::Thread>("external_mojo");
  base::SequenceBound<chromecast::external_mojo::ExternalMojoBroker> broker;

  if (chromecast::GetSwitchValueBoolean(switches::kInProcessBroker, false)) {
    // Set up the external Mojo Broker.
    broker_thread->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    broker = base::SequenceBound<chromecast::external_mojo::ExternalMojoBroker>(
        broker_thread->task_runner(),
        chromecast::external_mojo::GetBrokerPath());
    mojo::PendingRemote<chromecast::external_mojo::mojom::ExternalConnector>
        connector_remote;
    broker
        .AsyncCall(
            &chromecast::external_mojo::ExternalMojoBroker::BindConnector)
        .WithArgs(connector_remote.InitWithNewPipeAndPassReceiver());
    OnConnected(&state,
                chromecast::external_service_support::ExternalConnector::Create(
                    std::move(connector_remote)));
  } else {
    // Connect to existing Mojo broker.
    chromecast::external_service_support::ExternalConnector::Connect(
        chromecast::external_mojo::GetBrokerPath(),
        base::BindOnce(&OnConnected, &state));
  }

  run_loop.Run();
  base::ThreadPoolInstance::Get()->Shutdown();

  return 0;
}
