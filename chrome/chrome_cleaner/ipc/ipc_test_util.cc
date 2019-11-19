// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"

#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/win/win_util.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace chrome_cleaner {

namespace {

constexpr char kMojoPipeTokenSwitch[] = "mojo-pipe-token";

constexpr wchar_t kIPCTestUtilLogSuffix[] = L"ipc-test-util";

class MojoSandboxSetupHooks : public SandboxSetupHooks {
 public:
  explicit MojoSandboxSetupHooks(SandboxedParentProcess* parent_process)
      : parent_process_(parent_process) {}
  ~MojoSandboxSetupHooks() override = default;

  // SandboxSetupHooks

  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override {
    base::HandlesToInheritVector handles_to_inherit =
        parent_process_->extra_handles_to_inherit();
    parent_process_->CreateMojoPipe(command_line, &handles_to_inherit);
    for (HANDLE handle : handles_to_inherit)
      policy->AddHandleToShare(handle);
    return RESULT_CODE_SUCCESS;
  }

  ResultCode TargetSpawned(
      const base::Process& target_process,
      const base::win::ScopedHandle& target_thread) override {
    parent_process_->ConnectMojoPipe(target_process.Duplicate());
    return RESULT_CODE_SUCCESS;
  }

 private:
  SandboxedParentProcess* parent_process_;
};

}  // namespace

namespace internal {

base::FilePath::StringPieceType GetLogPathSuffix() {
  return kIPCTestUtilLogSuffix;
}

base::FilePath GetLogPath() {
  return ScopedLogging::GetLogFilePath(kIPCTestUtilLogSuffix);
}

bool DeleteChildProcessLogs() {
  // Delete the child process log file if existing.
  const base::FilePath log_path = GetLogPath();
  if (!base::DeleteFile(log_path, false)) {
    LOG(ERROR) << "Can't delete log file from previous run: "
               << log_path.value();
    return false;
  }
  return true;
}

void PrintChildProcessLogs() {
  const base::FilePath log_path = GetLogPath();
  if (log_path.empty()) {
    LOG(ERROR) << "Child process log path is empty";
    return;
  }

  if (!base::PathExists(log_path)) {
    LOG(ERROR) << "Child process log file doesn't exist";
    return;
  }

  // Collect the child process log file, and dump the contents, to help
  // debugging failures.
  std::string log_file_contents;
  if (!base::ReadFileToString(log_path, &log_file_contents)) {
    LOG(ERROR) << "Failed to read child process log file";
    return;
  }

  std::vector<base::StringPiece> lines =
      base::SplitStringPiece(log_file_contents, "\n", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  LOG(ERROR) << "Dumping child process logs";
  for (const auto& line : lines) {
    LOG(ERROR) << "Child process: " << line;
  }
}

}  // namespace internal

ParentProcess::ParentProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
    : command_line_(base::GetMultiProcessTestChildBaseCommandLine()),
      mojo_task_runner_(mojo_task_runner) {}

ParentProcess::~ParentProcess() {}

void ParentProcess::CreateImplOnIPCThread(
    mojo::ScopedMessagePipeHandle mojo_pipe) {
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ParentProcess::CreateImpl, base::RetainedRef(this),
                     base::Passed(&mojo_pipe)));
}

void ParentProcess::DestroyImplOnIPCThread() {
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ParentProcess::DestroyImpl, base::RetainedRef(this)));
}

void ParentProcess::AppendSwitch(const std::string& switch_string) {
  command_line_.AppendSwitch(switch_string);
}

void ParentProcess::AppendSwitch(const std::string& switch_string,
                                 const std::string& value) {
  command_line_.AppendSwitchASCII(switch_string, value);
}

void ParentProcess::AppendSwitchNative(const std::string& switch_string,
                                       const base::string16& value) {
  command_line_.AppendSwitchNative(switch_string, value);
}

void ParentProcess::AppendSwitchPath(const std::string& switch_string,
                                     const base::FilePath& value) {
  command_line_.AppendSwitchPath(switch_string, value);
}

void ParentProcess::AppendSwitchHandleToShare(const std::string& switch_string,
                                              HANDLE handle) {
  extra_handles_to_inherit_.push_back(handle);
  command_line_.AppendSwitchNative(
      switch_string, base::NumberToString16(base::win::HandleToUint32(handle)));
}

bool ParentProcess::LaunchConnectedChildProcess(
    const std::string& child_main_function,
    int32_t* exit_code) {
  return LaunchConnectedChildProcess(child_main_function,
                                     TestTimeouts::action_timeout(), exit_code);
}

bool ParentProcess::LaunchConnectedChildProcess(
    const std::string& child_main_function,
    base::TimeDelta timeout,
    int32_t* exit_code) {
  if (!internal::DeleteChildProcessLogs())
    return false;

  if (!PrepareAndLaunchTestChildProcess(child_main_function))
    return false;

  CreateImplOnIPCThread(std::move(mojo_pipe_));
  const bool success = base::WaitForMultiprocessTestChildExit(
      child_process_, timeout, exit_code);
  if (!success) {
    LOG(ERROR) << "Child process failed to terminate within " << timeout;
    child_process_.Terminate(/*exit_code=*/-1, /*wait=*/false);
  }
  DestroyImplOnIPCThread();

  if (!success || *exit_code != 0)
    internal::PrintChildProcessLogs();

  return success;
}

bool ParentProcess::PrepareAndLaunchTestChildProcess(
    const std::string& child_main_function) {
  base::LaunchOptions launch_options;
  launch_options.handles_to_inherit = extra_handles_to_inherit_;
  CreateMojoPipe(&command_line_, &launch_options.handles_to_inherit);

  base::Process child_process = base::SpawnMultiProcessTestChild(
      child_main_function, command_line_, launch_options);

  ConnectMojoPipe(std::move(child_process));
  return true;
}

void ParentProcess::CreateMojoPipe(
    base::CommandLine* command_line,
    base::HandlesToInheritVector* handles_to_inherit) {
  std::string mojo_pipe_token = base::NumberToString(base::RandUint64());
  mojo_pipe_ = outgoing_invitation_.AttachMessagePipe(mojo_pipe_token);
  command_line->AppendSwitchASCII(kMojoPipeTokenSwitch, mojo_pipe_token);
  mojo_channel_.PrepareToPassRemoteEndpoint(handles_to_inherit, command_line);
}

void ParentProcess::ConnectMojoPipe(base::Process process) {
  child_process_ = std::move(process);
  mojo::OutgoingInvitation::Send(std::move(outgoing_invitation_),
                                 child_process_.Handle(),
                                 mojo_channel_.TakeLocalEndpoint());
}

scoped_refptr<MojoTaskRunner> ParentProcess::mojo_task_runner() {
  return mojo_task_runner_;
}

SandboxedParentProcess::SandboxedParentProcess(
    scoped_refptr<MojoTaskRunner> mojo_task_runner)
    : ParentProcess(mojo_task_runner) {}

SandboxedParentProcess::~SandboxedParentProcess() {}

bool SandboxedParentProcess::PrepareAndLaunchTestChildProcess(
    const std::string& child_main_function) {
  MojoSandboxSetupHooks hooks(this);

  // This switch usage is copied from SpawnMultiProcessTestChild.
  //
  // We can't use SpawnMultiProcessTestChild here, because it uses
  // LaunchProcess internally and we need to start the test child using
  // StartSandboxTarget, which uses BrokerServices::SpawnTarget.
  if (!command_line_.HasSwitch(switches::kTestChildProcess))
    command_line_.AppendSwitchASCII(switches::kTestChildProcess,
                                    child_main_function);

  chrome_cleaner::ResultCode result_code =
      StartSandboxTarget(command_line_, &hooks, SandboxType::kTest);
  if (result_code != RESULT_CODE_SUCCESS) {
    LOG(ERROR) << "Failed to launch sandbox: " << result_code;
    return false;
  }
  return true;
}

ChildProcess::ChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
    : mojo_task_runner_(mojo_task_runner),
      command_line_(base::CommandLine::ForCurrentProcess()),
      scopped_logging_(new ScopedLogging(kIPCTestUtilLogSuffix)) {
  sandbox::TargetServices* target_services =
      sandbox::SandboxFactory::GetTargetServices();
  if (!target_services)
    return;

  sandbox::ResultCode result = target_services->Init();
  if (result != sandbox::SBOX_ALL_OK) {
    LOG(ERROR) << "Failed to initialize sandbox TargetServices: " << result;
    return;
  }
  target_services_initialized_ = true;
}

ChildProcess::~ChildProcess() {}

void ChildProcess::LowerToken() const {
  if (!target_services_initialized_)
    return;
  sandbox::TargetServices* target_services =
      sandbox::SandboxFactory::GetTargetServices();
  DCHECK(target_services);
  target_services->LowerToken();
}

mojo::ScopedMessagePipeHandle ChildProcess::CreateMessagePipeFromCommandLine() {
  auto channel_endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess());
  auto incoming_invitation =
      mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
  return incoming_invitation.ExtractMessagePipe(mojo_pipe_token());
}

std::string ChildProcess::mojo_pipe_token() const {
  return command_line_->GetSwitchValueASCII(kMojoPipeTokenSwitch);
}

ChromePromptIPCTestErrorHandler::ChromePromptIPCTestErrorHandler(
    base::OnceClosure on_closed,
    base::OnceClosure on_closed_after_done)
    : on_closed_(std::move(on_closed)),
      on_closed_after_done_(std::move(on_closed_after_done)) {}

ChromePromptIPCTestErrorHandler::~ChromePromptIPCTestErrorHandler() = default;

void ChromePromptIPCTestErrorHandler::OnConnectionClosed() {
  std::move(on_closed_).Run();
}

void ChromePromptIPCTestErrorHandler::OnConnectionClosedAfterDone() {
  std::move(on_closed_after_done_).Run();
}

}  // namespace chrome_cleaner
