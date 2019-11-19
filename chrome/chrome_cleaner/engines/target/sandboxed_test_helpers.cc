// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/sandboxed_test_helpers.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/task/single_thread_task_executor.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "chrome/chrome_cleaner/os/initializer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

const int SandboxChildProcess::kConnectionErrorExitCode = 0xDEAD;

// FakeEngineDelegate takes a base::Event that it signals once either
// Initialize, StartScan, or StartCleanup has been called to indicate when this
// class is fully functional (for either scanning or cleaning test). It does not
// invoke any actual engine commands.
class SandboxChildProcess::FakeEngineDelegate : public EngineDelegate {
 public:
  explicit FakeEngineDelegate(base::WaitableEvent* event) : event_(event) {}

  Engine::Name engine() const override { return Engine::TEST_ONLY; }

  void Initialize(const base::FilePath& log_directory_path,
                  scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
                  mojom::EngineCommands::InitializeCallback callback) override {
    privileged_file_calls_ = privileged_file_calls;
    event_->Signal();
    std::move(callback).Run(EngineResultCode::kSuccess);
  }

  uint32_t StartScan(
      const std::vector<UwSId>& enabled_uws,
      const std::vector<UwS::TraceLocation>& enabled_locations,
      bool include_details,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
      scoped_refptr<EngineScanResultsProxy> /*report_result_calls*/) override {
    privileged_file_calls_ = privileged_file_calls;
    privileged_scan_calls_ = privileged_scan_calls;
    event_->Signal();
    return EngineResultCode::kSuccess;
  }

  uint32_t StartCleanup(
      const std::vector<UwSId>& enabled_uws,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
      scoped_refptr<CleanerEngineRequestsProxy> privileged_removal_calls,
      scoped_refptr<EngineCleanupResultsProxy> /*report_result_calls*/)
      override {
    privileged_file_calls_ = privileged_file_calls;
    privileged_scan_calls_ = privileged_scan_calls;
    privileged_removal_calls_ = privileged_removal_calls;
    event_->Signal();
    return EngineResultCode::kSuccess;
  }

  uint32_t Finalize() override { return EngineResultCode::kSuccess; }

  scoped_refptr<EngineFileRequestsProxy> GetFileRequestsProxy() {
    return privileged_file_calls_;
  }

  scoped_refptr<EngineRequestsProxy> GetEngineRequestsProxy() {
    return privileged_scan_calls_;
  }

  scoped_refptr<CleanerEngineRequestsProxy> GetCleanerEngineRequestsProxy() {
    return privileged_removal_calls_;
  }

  void UnbindRequestsRemotes() {
    if (privileged_scan_calls_) {
      privileged_scan_calls_->UnbindRequestsRemote();
    }
    if (privileged_file_calls_) {
      privileged_file_calls_->UnbindRequestsRemote();
    }
  }

 private:
  ~FakeEngineDelegate() override = default;

  base::WaitableEvent* event_;
  scoped_refptr<EngineFileRequestsProxy> privileged_file_calls_;
  scoped_refptr<EngineRequestsProxy> privileged_scan_calls_;
  scoped_refptr<CleanerEngineRequestsProxy> privileged_removal_calls_;
};

SandboxChildProcess::SandboxChildProcess(
    scoped_refptr<MojoTaskRunner> mojo_task_runner)
    : ChildProcess(std::move(mojo_task_runner)) {
  // This must be called before accessing Mojo, because the parent process is
  // waiting on this and won't respond to Mojo calls.
  NotifyInitializationDone();

  mojo::ScopedMessagePipeHandle message_pipe_handle =
      CreateMessagePipeFromCommandLine();
  mojo::PendingReceiver<mojom::EngineCommands> engine_commands_receiver(
      std::move(message_pipe_handle));
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SandboxChildProcess::BindEngineCommandsReceiver,
                     base::Unretained(this),
                     base::Passed(&engine_commands_receiver), &event));
  event.Wait();
}

void SandboxChildProcess::SandboxChildProcess::BindEngineCommandsReceiver(
    mojo::PendingReceiver<mojom::EngineCommands> receiver,
    base::WaitableEvent* event) {
  fake_engine_delegate_ = base::MakeRefCounted<FakeEngineDelegate>(event);
  engine_commands_impl_ = std::make_unique<EngineCommandsImpl>(
      fake_engine_delegate_, std::move(receiver), mojo_task_runner_,
      /*error_handler=*/base::BindOnce(&EarlyExit, kConnectionErrorExitCode));
}

scoped_refptr<EngineFileRequestsProxy>
SandboxChildProcess::GetFileRequestsProxy() {
  return fake_engine_delegate_->GetFileRequestsProxy();
}

scoped_refptr<EngineRequestsProxy>
SandboxChildProcess::GetEngineRequestsProxy() {
  return fake_engine_delegate_->GetEngineRequestsProxy();
}

scoped_refptr<CleanerEngineRequestsProxy>
SandboxChildProcess::GetCleanerEngineRequestsProxy() {
  return fake_engine_delegate_->GetCleanerEngineRequestsProxy();
}

void SandboxChildProcess::UnbindRequestsRemotes() {
  base::SingleThreadTaskExecutor main_task_executor;
  base::RunLoop run_loop;
  if (GetCleanerEngineRequestsProxy() != nullptr) {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CleanerEngineRequestsProxy::UnbindRequestsRemote,
                       GetCleanerEngineRequestsProxy()));
  }

  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeEngineDelegate::UnbindRequestsRemotes,
                                fake_engine_delegate_));

  mojo_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                      run_loop.QuitClosure());
  run_loop.Run();
}

SandboxChildProcess::~SandboxChildProcess() {
  // |engine_commands_impl_| must be destroyed on the Mojo thread or it will
  // crash.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<EngineCommandsImpl> commands) {
                       commands.reset();
                     },
                     base::Passed(&engine_commands_impl_)));
}

}  // namespace chrome_cleaner
