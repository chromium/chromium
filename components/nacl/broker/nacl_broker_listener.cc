// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/broker/nacl_broker_listener.h"

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/win_util.h"
#include "components/nacl/common/nacl_cmd_line.h"
#include "components/nacl/common/nacl_debug_exception_handler_win.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_service.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init_win.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy.h"

#include <windows.h>

namespace {

void SendReply(IPC::Channel* channel, int32_t pid, bool result) {
  channel->Send(new NaClProcessMsg_DebugExceptionHandlerLaunched(pid, result));
}

}  // namespace

NaClBrokerListener::NaClBrokerListener() = default;

NaClBrokerListener::~NaClBrokerListener() = default;

void NaClBrokerListener::Listen() {
  NaClService service(base::SingleThreadTaskRunner::GetCurrentDefault());
  channel_ = IPC::Channel::CreateClient(
      service.TakeChannelPipe().release(), this,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  CHECK(channel_->Connect());
  run_loop_.Run();
}

sandbox::mojom::Sandbox NaClBrokerListener::GetSandboxType() {
  return sandbox::mojom::Sandbox::kPpapi;
}

std::string NaClBrokerListener::GetSandboxTag() {
  return sandbox::policy::SandboxWin::GetSandboxTagForDelegate(
      "nacl-broker-listener", GetSandboxType());
}

void NaClBrokerListener::OnChannelConnected(int32_t peer_pid) {
  browser_process_ = base::Process::OpenWithExtraPrivileges(peer_pid);
  CHECK(browser_process_.IsValid());
}

bool NaClBrokerListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClBrokerListener, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LaunchLoaderThroughBroker,
                        OnLaunchLoaderThroughBroker)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LaunchDebugExceptionHandler,
                        OnLaunchDebugExceptionHandler)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_StopBroker, OnStopBroker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClBrokerListener::OnChannelError() {
  // The browser died unexpectedly, quit to avoid a zombie process.
  run_loop_.QuitWhenIdle();
}

void NaClBrokerListener::OnLaunchLoaderThroughBroker(
    int launch_id,
    mojo::MessagePipeHandle ipc_channel_handle) {
  base::ProcessHandle loader_handle_in_browser = 0;

  // Create the path to the nacl broker/loader executable - it's the executable
  // this code is running in.
  base::FilePath exe_path;
  base::PathService::Get(base::FILE_EXE, &exe_path);
  if (!exe_path.empty()) {
    base::CommandLine* cmd_line = new base::CommandLine(exe_path);
    nacl::CopyNaClCommandLineArguments(cmd_line);

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kNaClLoaderProcess);

    // Mojo IPC setup.
    mojo::PlatformChannel channel;
    base::HandlesToInheritVector handles;
    channel.PrepareToPassRemoteEndpoint(&handles, cmd_line);

    mojo::OutgoingInvitation invitation;
    MojoResult fuse_result = mojo::FuseMessagePipes(
        mojo::ScopedMessagePipeHandle(ipc_channel_handle),
        invitation.AttachMessagePipe(0));
    DCHECK_EQ(MOJO_RESULT_OK, fuse_result);

    base::Process loader_process;
    sandbox::ResultCode result = content::StartSandboxedProcess(
        this, *cmd_line, handles, &loader_process);

    if (result == sandbox::SBOX_ALL_OK) {
      mojo::OutgoingInvitation::Send(std::move(invitation),
                                     loader_process.Handle(),
                                     channel.TakeLocalEndpoint());

      // Note: PROCESS_DUP_HANDLE is necessary here, because:
      // 1) The current process is the broker, which is the loader's parent.
      // 2) The browser is not the loader's parent, and so only gets the
      //    access rights we confer here.
      // 3) The browser calls DuplicateHandle to set up communications with
      //    the loader.
      // 4) The target process handle to DuplicateHandle needs to have
      //    PROCESS_DUP_HANDLE access rights.
      DuplicateHandle(
          ::GetCurrentProcess(), loader_process.Handle(),
          browser_process_.Handle(), &loader_handle_in_browser,
          PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE,
          FALSE, 0);
    }
  }

  channel_->Send(
      new NaClProcessMsg_LoaderLaunched(launch_id, loader_handle_in_browser));
}

void NaClBrokerListener::OnLaunchDebugExceptionHandler(
    int32_t pid,
    base::ProcessHandle process_handle,
    const std::string& startup_info) {
  NaClStartDebugExceptionHandlerThread(
      base::Process(process_handle), startup_info,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindRepeating(SendReply, channel_.get(), pid));
}

void NaClBrokerListener::OnStopBroker() {
  run_loop_.QuitWhenIdle();
}
