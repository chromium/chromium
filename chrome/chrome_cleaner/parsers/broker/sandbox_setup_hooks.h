// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_BROKER_SANDBOX_SETUP_HOOKS_H_
#define CHROME_CHROME_CLEANER_PARSERS_BROKER_SANDBOX_SETUP_HOOKS_H_

#include <memory>

#include "base/command_line.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

using RemoteParserPtr =
    std::unique_ptr<mojo::Remote<mojom::Parser>, base::OnTaskRunnerDeleter>;

// Hooks to spawn a new sandboxed Parser process and bind a Mojo interface
// pointer to the sandboxed implementation.
class ParserSandboxSetupHooks : public MojoSandboxSetupHooks {
 public:
  ParserSandboxSetupHooks(scoped_refptr<MojoTaskRunner> mojo_task_runner,
                          base::OnceClosure connection_error_handler);
  ~ParserSandboxSetupHooks() override;

  // Transfers ownership of |parser_| to the caller.
  RemoteParserPtr TakeParserRemote();

  // SandboxSetupHooks
  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override;

 private:
  void BindParserRemote(mojo::ScopedMessagePipeHandle pipe_handle,
                        mojo::Remote<mojom::Parser>* parser);

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  base::OnceClosure connection_error_handler_;

  RemoteParserPtr parser_;

  DISALLOW_COPY_AND_ASSIGN(ParserSandboxSetupHooks);
};

// Spawn a sandboxed process with type kParser, and return the bound
// |parser|.
ResultCode SpawnParserSandbox(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    RemoteParserPtr* parser);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_BROKER_SANDBOX_SETUP_HOOKS_H_
