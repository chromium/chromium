// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_broker_host_win.h"

#include <windows.h>

#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "components/nacl/browser/nacl_broker_service_win.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/nacl_cmd_line.h"
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace {
// NOTE: changes to this class need to be reviewed by the security team.
class NaClBrokerSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  NaClBrokerSandboxedProcessLauncherDelegate() {}

  NaClBrokerSandboxedProcessLauncherDelegate(
      const NaClBrokerSandboxedProcessLauncherDelegate&) = delete;
  NaClBrokerSandboxedProcessLauncherDelegate& operator=(
      const NaClBrokerSandboxedProcessLauncherDelegate&) = delete;

  sandbox::mojom::Sandbox GetSandboxType() override {
    return sandbox::mojom::Sandbox::kNoSandbox;
  }

  std::string GetSandboxTag() override {
    // kNoSandbox does not use a TargetPolicy, if the sandbox type is changed
    // then provide a unique tag here.
    return "";
  }
};
}  // namespace

namespace nacl {

NaClBrokerHost::NaClBrokerHost() : is_terminating_(false) {
}

NaClBrokerHost::~NaClBrokerHost() {
}

bool NaClBrokerHost::Init() {
  DCHECK(!process_);
  process_ = content::BrowserChildProcessHost::Create(
      static_cast<content::ProcessType>(PROCESS_TYPE_NACL_BROKER), this,
      content::ChildProcessHost::IpcMode::kLegacy);
  process_->SetMetricsName("NaCl Broker");
  process_->GetHost()->CreateChannelMojo();

  // Create the path to the nacl broker/loader executable.
  base::FilePath nacl_path;
  if (!NaClBrowser::GetInstance()->GetNaCl64ExePath(&nacl_path))
    return false;

  base::CommandLine* cmd_line = new base::CommandLine(nacl_path);
  CopyNaClCommandLineArguments(cmd_line);

  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kNaClBrokerProcess);
  if (NaClBrowser::GetDelegate()->DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  process_->Launch(
      std::make_unique<NaClBrokerSandboxedProcessLauncherDelegate>(),
      base::WrapUnique(cmd_line), true);
  return true;
}

bool NaClBrokerHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClBrokerHost, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_LoaderLaunched, OnLoaderLaunched)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_DebugExceptionHandlerLaunched,
                        OnDebugExceptionHandlerLaunched)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool NaClBrokerHost::LaunchLoader(
    int launch_id,
    mojo::ScopedMessagePipeHandle ipc_channel_handle) {
  return process_->Send(new NaClProcessMsg_LaunchLoaderThroughBroker(
      launch_id, ipc_channel_handle.release()));
}

void NaClBrokerHost::OnLoaderLaunched(int launch_id,
                                      base::ProcessHandle handle) {
  NaClBrokerService::GetInstance()->OnLoaderLaunched(launch_id,
                                                     base::Process(handle));
}

bool NaClBrokerHost::LaunchDebugExceptionHandler(
    int32_t pid,
    base::ProcessHandle process_handle,
    const std::string& startup_info) {
  base::ProcessHandle broker_process =
      process_->GetData().GetProcess().Handle();
  base::ProcessHandle handle_in_broker_process;
  if (!DuplicateHandle(::GetCurrentProcess(), process_handle,
                       broker_process, &handle_in_broker_process,
                       0, /* bInheritHandle= */ FALSE, DUPLICATE_SAME_ACCESS))
    return false;
  return process_->Send(new NaClProcessMsg_LaunchDebugExceptionHandler(
      pid, handle_in_broker_process, startup_info));
}

void NaClBrokerHost::OnDebugExceptionHandlerLaunched(int32_t pid,
                                                     bool success) {
  NaClBrokerService::GetInstance()->OnDebugExceptionHandlerLaunched(pid,
                                                                    success);
}

void NaClBrokerHost::StopBroker() {
  is_terminating_ = true;
  process_->Send(new NaClProcessMsg_StopBroker());
}

}  // namespace nacl
