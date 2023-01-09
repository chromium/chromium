// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_IPC_TEST_UTIL_H_
#define CHROME_CHROME_CLEANER_IPC_IPC_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/test/child_process_logger.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

typedef base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle mojo_pipe)>
    CreateImplCallback;

class ParentProcess : public base::RefCountedThreadSafe<ParentProcess> {
 public:
  explicit ParentProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner);

  bool LaunchConnectedChildProcess(const std::string& child_main_function,
                                   int32_t* exit_code);

  bool LaunchConnectedChildProcess(const std::string& child_main_function,
                                   base::TimeDelta timeout,
                                   int32_t* exit_code);

  void AppendSwitch(const std::string& switch_string);
  void AppendSwitch(const std::string& switch_string, const std::string& value);
  void AppendSwitchNative(const std::string& switch_string,
                          const std::wstring& value);
  void AppendSwitchPath(const std::string& switch_string,
                        const base::FilePath& value);
  void AppendSwitchHandleToShare(const std::string& switch_string,
                                 HANDLE handle);

  // The following methods are called during the launch sequence. They are
  // public so they can be called from helper classes.
  void CreateImplOnIPCThread(mojo::ScopedMessagePipeHandle mojo_pipe);
  void DestroyImplOnIPCThread();
  void CreateMojoPipe(base::CommandLine* command_line,
                      base::HandlesToInheritVector* handles_to_inherit);
  void ConnectMojoPipe(base::Process child_process);

  base::HandlesToInheritVector extra_handles_to_inherit() const {
    return extra_handles_to_inherit_;
  }

  const ChildProcessLogger& child_process_logger() const {
    return child_process_logger_;
  }

 protected:
  friend base::RefCountedThreadSafe<ParentProcess>;
  virtual ~ParentProcess();

  // This is called on the IPC thread.
  virtual void CreateImpl(mojo::ScopedMessagePipeHandle mojo_pipe) = 0;
  virtual void DestroyImpl() = 0;

  // Subclasses can override this to launch the child in different ways, such
  // as in the sandbox. Subclasses should call CreateMojoPipe before the
  // subprocess is spawned and ConnectMojoPipe afterward.
  virtual bool PrepareAndLaunchTestChildProcess(
      const std::string& child_main_function);

  scoped_refptr<MojoTaskRunner> mojo_task_runner();

  base::CommandLine command_line_;
  base::HandlesToInheritVector extra_handles_to_inherit_;
  ChildProcessLogger child_process_logger_;

 private:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  mojo::OutgoingInvitation outgoing_invitation_;
  mojo::ScopedMessagePipeHandle mojo_pipe_;
  mojo::PlatformChannel mojo_channel_;
  base::Process child_process_;
};

class SandboxedParentProcess : public ParentProcess {
 public:
  explicit SandboxedParentProcess(
      scoped_refptr<MojoTaskRunner> mojo_task_runner);

 protected:
  friend base::RefCountedThreadSafe<SandboxedParentProcess>;
  ~SandboxedParentProcess() override;

  bool PrepareAndLaunchTestChildProcess(
      const std::string& child_main_function) override;
};

class ChildProcess : public base::RefCountedThreadSafe<ChildProcess> {
 public:
  explicit ChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner);

  mojo::ScopedMessagePipeHandle CreateMessagePipeFromCommandLine();

  std::string mojo_pipe_token() const;

  const base::CommandLine& command_line() const { return *command_line_; }

  // This will drop all privileges if the child process is running in a
  // sandbox. If not, it will do nothing.
  void LowerToken() const;

 protected:
  friend base::RefCountedThreadSafe<ChildProcess>;
  virtual ~ChildProcess();

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;

 private:
  base::CommandLine* command_line_;

  // This will be true iff the process is running in a sandbox and
  // TargetServices was initialized successfully.
  bool target_services_initialized_ = false;
};

class ChromePromptIPCTestErrorHandler : public ChromePromptIPC::ErrorHandler {
 public:
  ChromePromptIPCTestErrorHandler(base::OnceClosure on_closed,
                                  base::OnceClosure on_closed_after_done);

  ~ChromePromptIPCTestErrorHandler() override;

  void OnConnectionClosed() override;
  void OnConnectionClosedAfterDone() override;

 private:
  base::OnceClosure on_closed_;
  base::OnceClosure on_closed_after_done_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_IPC_TEST_UTIL_H_
