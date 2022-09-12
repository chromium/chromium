// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chromecast/external_mojo/external_service_support/external_connector_impl.h"
#include "chromecast/external_mojo/external_service_support/process_setup.h"
#include "chromecast/external_mojo/external_service_support/tracing_client.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "chromecast/external_mojo/public/cpp/external_mojo_broker.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

// Standalone Mojo broker process.

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  chromecast::external_service_support::CommonProcessInitialization(argc, argv);

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;

  mojo::core::Configuration mojo_config;
  mojo_config.is_broker_process = true;
  mojo::core::Init(mojo_config);

  mojo::core::ScopedIPCSupport ipc_support(
      io_task_executor.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "StandaloneMojoBroker");

  chromecast::external_mojo::ExternalMojoBroker broker(
      chromecast::external_mojo::GetBrokerPath());

  chromecast::external_service_support::ExternalConnectorImpl tracing_connector(
      broker.CreateConnector());
  auto tracing_client =
      chromecast::external_service_support::TracingClient::Create(
          &tracing_connector);

  run_loop.Run();
  base::ThreadPoolInstance::Get()->Shutdown();

  return 0;
}
