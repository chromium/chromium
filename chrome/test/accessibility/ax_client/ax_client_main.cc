// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <utility>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/notreached.h"
#include "base/process/memory.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/test/accessibility/ax_client/ax_client.h"
#include "chrome/test/accessibility/ax_client/ax_client.test-mojom-forward.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace {

ax_client::AxClient::ClientApi GetClientApi(
    const base::CommandLine& command_line) {
  int value;
  if (auto value_str = command_line.GetSwitchValueNative("client-api");
      base::StringToInt(value_str, &value)) {
    switch (value) {
      case static_cast<int>(ax_client::AxClient::ClientApi::kUiAutomation):
        return ax_client::AxClient::ClientApi::kUiAutomation;
      case static_cast<int>(ax_client::AxClient::ClientApi::kIAccessible2):
        return ax_client::AxClient::ClientApi::kIAccessible2;
      default:
        NOTREACHED() << "--client-api=" << value << " out of bounds";
    }
  } else {
    NOTREACHED() << "--client-api=" << value_str << " invalid";
  }
}

}  // namespace

extern "C" int wmain(int argc, const wchar_t* const argv) {
  base::AtExitManager at_exit;

  base::CommandLine::Init(/*argc=*/0, /*argv=*/nullptr);
  auto& command_line = *base::CommandLine::ForCurrentProcess();

  logging::InitLogging({.logging_dest = logging::LOG_TO_STDERR});
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(command_line);

  const ax_client::AxClient::ClientApi client_api = GetClientApi(command_line);

  // Basic Mojo initialization for a new process.
  mojo::core::Init();
  base::Thread ipc_thread("ipc");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  // Accept the invitation for the AxClient instance.
  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          command_line));
  mojo::ScopedMessagePipeHandle pipe =
      invitation.ExtractMessagePipe(/*name=*/0);

  // Initialize COM on this main thread.
  std::optional<base::win::ScopedCOMInitializer> com_initializer;
  switch (client_api) {
    case ax_client::AxClient::ClientApi::kUiAutomation:
      // MTA for UI Automation.
      com_initializer.emplace(base::win::ScopedCOMInitializer::kMTA);
      break;
    case ax_client::AxClient::ClientApi::kIAccessible2:
      // STA for MSAA/IA2.
      com_initializer.emplace();
      break;
  }
  CHECK(com_initializer->Succeeded());

  // Run a task executor on this main thread -- it will host the AxClient.
  // MSAA/IA2 requires a UI message pump since this thread hosts the client's
  // WinEvent hook.
  base::SingleThreadTaskExecutor task_executor(
      client_api == ax_client::AxClient::ClientApi::kIAccessible2
          ? base::MessagePumpType::UI
          : base::MessagePumpType::DEFAULT,
      /*is_main_thread=*/true);

  base::RunLoop run_loop;

  // Create and bind the client such that the RunLoop quits upon disconnect.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ax_client::AxClient>(client_api, run_loop.QuitClosure()),
      mojo::PendingReceiver<ax_client::mojom::AxClient>(std::move(pipe)));

  run_loop.Run();

  return 0;
}
