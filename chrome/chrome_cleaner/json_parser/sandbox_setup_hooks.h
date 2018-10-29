// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_JSON_PARSER_SANDBOX_SETUP_HOOKS_H_
#define CHROME_CHROME_CLEANER_JSON_PARSER_SANDBOX_SETUP_HOOKS_H_

#include <memory>

#include "base/command_line.h"
#include "chrome/chrome_cleaner/interfaces/json_parser.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

using UniqueJsonParserPtr =
    std::unique_ptr<mojom::JsonParserPtr, base::OnTaskRunnerDeleter>;

// Hooks to spawn a new sandboxed JSON parser process and bind a Mojo interface
// pointer to the sandboxed implementation.
class JsonParserSandboxSetupHooks : public MojoSandboxSetupHooks {
 public:
  JsonParserSandboxSetupHooks(scoped_refptr<MojoTaskRunner> mojo_task_runner,
                              base::OnceClosure connection_error_handler);
  ~JsonParserSandboxSetupHooks() override;

  // Transfers ownership of |json_parser_ptr_| to the caller.
  UniqueJsonParserPtr TakeJsonParserPtr();

  // SandboxSetupHooks
  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override;

 private:
  void BindJsonParserPtr(mojo::ScopedMessagePipeHandle pipe_handle,
                         mojom::JsonParserPtr* json_parser_ptr);

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  base::OnceClosure connection_error_handler_;

  UniqueJsonParserPtr json_parser_ptr_;

  DISALLOW_COPY_AND_ASSIGN(JsonParserSandboxSetupHooks);
};

// Spawn a sandboxed process with type kJsonParser, and return the bound
// |json_parser_ptr|.
ResultCode SpawnJsonParserSandbox(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    UniqueJsonParserPtr* json_parser_ptr);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_JSON_PARSER_SANDBOX_SETUP_HOOKS_H_
