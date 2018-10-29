// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/json_parser/sandbox_setup_hooks.h"

#include <utility>

#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"

namespace chrome_cleaner {

JsonParserSandboxSetupHooks::JsonParserSandboxSetupHooks(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    base::OnceClosure connection_error_handler)
    : mojo_task_runner_(mojo_task_runner),
      connection_error_handler_(std::move(connection_error_handler)),
      json_parser_ptr_(new mojom::JsonParserPtr(),
                       base::OnTaskRunnerDeleter(mojo_task_runner_)) {}

JsonParserSandboxSetupHooks::~JsonParserSandboxSetupHooks() = default;

ResultCode JsonParserSandboxSetupHooks::UpdateSandboxPolicy(
    sandbox::TargetPolicy* policy,
    base::CommandLine* command_line) {
  // Unretained reference is safe because the json_parser_ptr is taken by the
  // caller and is expected to retain it for the life of the sandboxed process.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JsonParserSandboxSetupHooks::BindJsonParserPtr,
                                base::Unretained(this),
                                SetupSandboxMessagePipe(policy, command_line),
                                base::Unretained(json_parser_ptr_.get())));

  return RESULT_CODE_SUCCESS;
}

void JsonParserSandboxSetupHooks::BindJsonParserPtr(
    mojo::ScopedMessagePipeHandle pipe_handle,
    mojom::JsonParserPtr* json_parser_ptr) {
  json_parser_ptr->Bind(mojom::JsonParserPtrInfo(std::move(pipe_handle), 0));
  json_parser_ptr->set_connection_error_handler(
      std::move(connection_error_handler_));
}

UniqueJsonParserPtr JsonParserSandboxSetupHooks::TakeJsonParserPtr() {
  return std::move(json_parser_ptr_);
}

ResultCode SpawnJsonParserSandbox(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    UniqueJsonParserPtr* json_parser_ptr) {
  // Call |connection_error_callback| with json parser sandbox type.
  auto error_handler =
      base::BindOnce(connection_error_callback, SandboxType::kJsonParser);
  JsonParserSandboxSetupHooks setup_hooks(mojo_task_runner,
                                          std::move(error_handler));
  ResultCode result_code = SpawnSandbox(&setup_hooks, SandboxType::kJsonParser);
  *json_parser_ptr = setup_hooks.TakeJsonParserPtr();

  return result_code;
}

}  // namespace chrome_cleaner
