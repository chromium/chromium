// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/sandbox_setup.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/engines/target/engine_commands_impl.h"
#include "chrome/chrome_cleaner/engines/target/libraries.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chrome_cleaner {

namespace {

class EngineMojoSandboxTargetHooks : public MojoSandboxTargetHooks {
 public:
  EngineMojoSandboxTargetHooks(scoped_refptr<EngineDelegate> engine_delegate,
                               MojoTaskRunner* mojo_task_runner);
  ~EngineMojoSandboxTargetHooks() override;

  void BindEngineCommandsReceiver(
      mojo::PendingReceiver<mojom::EngineCommands> receiver);

  // SandboxTargetHooks

  ResultCode TargetDroppedPrivileges(
      const base::CommandLine& command_line) override;

 private:
  scoped_refptr<EngineDelegate> engine_delegate_;
  MojoTaskRunner* mojo_task_runner_;

  base::SingleThreadTaskExecutor single_thread_task_executor_;

  std::unique_ptr<EngineCommandsImpl> engine_commands_impl_;

  DISALLOW_COPY_AND_ASSIGN(EngineMojoSandboxTargetHooks);
};

EngineMojoSandboxTargetHooks::EngineMojoSandboxTargetHooks(
    scoped_refptr<EngineDelegate> engine_delegate,
    MojoTaskRunner* mojo_task_runner)
    : engine_delegate_(engine_delegate), mojo_task_runner_(mojo_task_runner) {}

EngineMojoSandboxTargetHooks::~EngineMojoSandboxTargetHooks() {
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<EngineCommandsImpl> commands) {
                       commands.reset();
                     },
                     base::Passed(&engine_commands_impl_)));
}

ResultCode EngineMojoSandboxTargetHooks::TargetDroppedPrivileges(
    const base::CommandLine& command_line) {
  // Connect to the Mojo message pipe from the parent process.
  mojo::PendingReceiver<mojom::EngineCommands> receiver(
      ExtractSandboxMessagePipe(command_line));

  // This loop will run forever. Once the communication channel with the broker
  // process is broken, mojo error handler will abort this process.
  base::RunLoop run_loop;
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EngineMojoSandboxTargetHooks::BindEngineCommandsReceiver,
                     base::Unretained(this), base::Passed(&receiver)));

  run_loop.Run();
  return RESULT_CODE_SUCCESS;
}

void EngineMojoSandboxTargetHooks::BindEngineCommandsReceiver(
    mojo::PendingReceiver<mojom::EngineCommands> receiver) {
  // If the connection dies, the parent process has terminated unexpectedly.
  // Exit immediately. The child process should be killed automatically if the
  // parent dies, so this is just a fallback. The exit code is arbitrary since
  // there's no parent process to collect and report it.
  auto error_handler = base::BindOnce(&EarlyExit, 1);

  engine_commands_impl_ = std::make_unique<EngineCommandsImpl>(
      std::move(engine_delegate_), std::move(receiver), mojo_task_runner_,
      std::move(error_handler));
}

}  // namespace

ResultCode RunEngineSandboxTarget(
    scoped_refptr<EngineDelegate> engine_delegate,
    const base::CommandLine& command_line,
    sandbox::TargetServices* sandbox_target_services) {
  // Extract the libraries to the same directory as the executable.
  base::FilePath extraction_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &extraction_dir));
  if (!LoadAndValidateLibraries(engine_delegate->engine(), extraction_dir)) {
    NOTREACHED() << "Binary signature validation failed";
    return RESULT_CODE_SIGNATURE_VERIFICATION_FAILED;
  }

  scoped_refptr<MojoTaskRunner> sandbox_mojo_task_runner =
      MojoTaskRunner::Create();

  EngineMojoSandboxTargetHooks mojo_target_hooks(
      std::move(engine_delegate), sandbox_mojo_task_runner.get());
  return RunSandboxTarget(command_line, sandbox_target_services,
                          &mojo_target_hooks);
}

}  // namespace chrome_cleaner
