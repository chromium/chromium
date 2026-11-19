// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/policy/test_support/fake_dmserver.h"

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("fake_dmserver");
  base::CommandLine::Init(argc, argv);

  fakedms::FakeServerConfig config;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  switch (fakedms::ParseFlags(*command_line, config)) {
    case fakedms::ParseFlagsResult::kSuccess:
      break;
    case fakedms::ParseFlagsResult::kHelp:
      fakedms::PrintHelp();
      return 0;
    case fakedms::ParseFlagsResult::kError:
      return 1;
  }
  fakedms::InitLogging(config.log_path, config.log_to_console,
                       config.min_log_level);

  base::RunLoop run_loop;
  fakedms::FakeDMServer policy_test_server(
      config.policy_blob_path, config.client_state_path,
      config.grpc_unix_socket_uri,
      base::SequencedTaskRunner::GetCurrentDefault(), run_loop.QuitClosure());
  if (!policy_test_server.StartFakeServer(config.port, config.fcm_port)) {
    return 1;
  }
  if (config.startup_pipe.is_valid()) {
    if (!policy_test_server.WriteURLToPipe(std::move(config.startup_pipe))) {
      return 1;
    }
  }
  run_loop.Run();
  base::ThreadPoolInstance::Get()->Shutdown();
  return 0;
}
