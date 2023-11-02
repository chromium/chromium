// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_MOJO_SANDBOX_HOOKS_H_
#define CHROME_CHROME_CLEANER_IPC_MOJO_SANDBOX_HOOKS_H_

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace chrome_cleaner {

class MojoSandboxSetupHooks : public SandboxSetupHooks {
 public:
  MojoSandboxSetupHooks();

  MojoSandboxSetupHooks(const MojoSandboxSetupHooks&) = delete;
  MojoSandboxSetupHooks& operator=(const MojoSandboxSetupHooks&) = delete;

  ~MojoSandboxSetupHooks() override;

 protected:
  mojo::ScopedMessagePipeHandle SetupSandboxMessagePipe(
      sandbox::TargetPolicy* policy,
      base::CommandLine* command_line);

  // SandboxSetupHooks

  ResultCode TargetSpawned(
      const base::Process& target_process,
      const base::win::ScopedHandle& target_thread) override;

  void SetupFailed() override;

 private:
  void ReportProcessLaunchAttempt();

  bool message_pipe_was_created_ = false;
  bool process_launch_attempt_reported_ = false;
  mojo::OutgoingInvitation outgoing_invitation_;
  mojo::PlatformChannel mojo_channel_;
};

class MojoSandboxTargetHooks : public SandboxTargetHooks {
 public:
  MojoSandboxTargetHooks();

  MojoSandboxTargetHooks(const MojoSandboxTargetHooks&) = delete;
  MojoSandboxTargetHooks& operator=(const MojoSandboxTargetHooks&) = delete;

  ~MojoSandboxTargetHooks() override;

 protected:
  mojo::ScopedMessagePipeHandle ExtractSandboxMessagePipe(
      const base::CommandLine& command_line);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_MOJO_SANDBOX_HOOKS_H_
