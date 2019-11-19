// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/target/sandbox_setup.h"

#include <utility>

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "chrome/chrome_cleaner/parsers/target/parser_impl.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chrome_cleaner {

namespace {

class ParserSandboxTargetHooks : public MojoSandboxTargetHooks {
 public:
  explicit ParserSandboxTargetHooks(MojoTaskRunner* mojo_task_runner)
      : mojo_task_runner_(mojo_task_runner) {}

  ~ParserSandboxTargetHooks() override {
    // Delete the mojo objects on the IPC thread.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<ParserImpl> parser_impl) {
                         parser_impl.reset();
                       },
                       base::Passed(&parser_impl_)));
  }

  // SandboxTargetHooks
  ResultCode TargetDroppedPrivileges(
      const base::CommandLine& command_line) override {
    mojo::PendingReceiver<mojom::Parser> receiver(
        ExtractSandboxMessagePipe(command_line));

    // This loop will run forever. Once the communication channel with the
    // broker process is broken, mojo error handler will abort this process.
    base::RunLoop run_loop;
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ParserSandboxTargetHooks::CreateParserImpl,
                       base::Unretained(this), base::Passed(&receiver)));
    run_loop.Run();
    return RESULT_CODE_SUCCESS;
  }

 private:
  void CreateParserImpl(mojo::PendingReceiver<mojom::Parser> receiver) {
    parser_impl_ = std::make_unique<ParserImpl>(std::move(receiver),
                                                base::BindOnce(&EarlyExit, 1));
  }

  MojoTaskRunner* mojo_task_runner_;
  base::SingleThreadTaskExecutor main_thread_task_executor_;
  std::unique_ptr<ParserImpl> parser_impl_;

  DISALLOW_COPY_AND_ASSIGN(ParserSandboxTargetHooks);
};

}  // namespace

ResultCode RunParserSandboxTarget(const base::CommandLine& command_line,
                                  sandbox::TargetServices* target_services) {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  ParserSandboxTargetHooks target_hooks(mojo_task_runner.get());

  return RunSandboxTarget(command_line, target_services, &target_hooks);
}

}  // namespace chrome_cleaner
