// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"

namespace chrome_cleaner {

MojoSandboxSetupHooks::MojoSandboxSetupHooks() = default;

MojoSandboxSetupHooks::~MojoSandboxSetupHooks() = default;

mojo::ScopedMessagePipeHandle MojoSandboxSetupHooks::SetupSandboxMessagePipe(
    sandbox::TargetPolicy* policy,
    base::CommandLine* command_line) {
  DCHECK(policy);
  DCHECK(command_line);

  std::string pipe_token = base::NumberToString(base::RandUint64());
  mojo::ScopedMessagePipeHandle mojo_pipe =
      outgoing_invitation_.AttachMessagePipe(pipe_token);
  command_line->AppendSwitchASCII(kSandboxMojoPipeTokenSwitch, pipe_token);

  base::HandlesToInheritVector handles_to_inherit;
  mojo_channel_.PrepareToPassRemoteEndpoint(&handles_to_inherit, command_line);
  for (HANDLE handle : handles_to_inherit)
    policy->AddHandleToShare(handle);

  message_pipe_was_created_ = true;
  return mojo_pipe;
}

ResultCode MojoSandboxSetupHooks::TargetSpawned(
    const base::Process& target_process,
    const base::win::ScopedHandle& target_thread) {
  DCHECK(message_pipe_was_created_);

  ReportProcessLaunchAttempt();

  // TODO(joenotcharles): Hook up the |error_callback| parameter of Send to a
  // function that logs a security warning and exits. This callback will be
  // called when an invalid message is written to the pipe.
  mojo::OutgoingInvitation::Send(std::move(outgoing_invitation_),
                                 target_process.Handle(),
                                 mojo_channel_.TakeLocalEndpoint());

  return RESULT_CODE_SUCCESS;
}

void MojoSandboxSetupHooks::SetupFailed() {
  if (process_launch_attempt_reported_) {
    // Setup failed after calling TargetSpawned, so don't report the launch
    // attempt again.
    return;
  }

  ReportProcessLaunchAttempt();
}

void MojoSandboxSetupHooks::ReportProcessLaunchAttempt() {
  DCHECK(!process_launch_attempt_reported_);
  mojo_channel_.RemoteProcessLaunchAttempted();
  process_launch_attempt_reported_ = true;
}

MojoSandboxTargetHooks::MojoSandboxTargetHooks() = default;

MojoSandboxTargetHooks::~MojoSandboxTargetHooks() = default;

mojo::ScopedMessagePipeHandle MojoSandboxTargetHooks::ExtractSandboxMessagePipe(
    const base::CommandLine& command_line) {
  auto channel_endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
  auto incoming_invitation =
      mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
  return incoming_invitation.ExtractMessagePipe(
      command_line.GetSwitchValueASCII(kSandboxMojoPipeTokenSwitch));
}

}  // namespace chrome_cleaner
