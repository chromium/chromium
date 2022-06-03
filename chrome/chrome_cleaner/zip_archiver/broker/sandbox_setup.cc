// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/zip_archiver/broker/sandbox_setup.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

namespace {

void BindZipArchiverRemote(mojo::Remote<mojom::ZipArchiver>* zip_archiver,
                           mojo::ScopedMessagePipeHandle pipe_handle,
                           base::OnceClosure connection_error_handler) {
  DCHECK(zip_archiver);

  zip_archiver->Bind(
      mojo::PendingRemote<mojom::ZipArchiver>(std::move(pipe_handle), 0));
  zip_archiver->set_disconnect_handler(std::move(connection_error_handler));
}

}  // namespace

ZipArchiverSandboxSetupHooks::ZipArchiverSandboxSetupHooks(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    base::OnceClosure connection_error_handler)
    : mojo_task_runner_(mojo_task_runner),
      connection_error_handler_(std::move(connection_error_handler)),
      // Manually use |new| here because |make_unique| doesn't work with
      // custom deleter.
      zip_archiver_(new mojo::Remote<mojom::ZipArchiver>(),
                    base::OnTaskRunnerDeleter(mojo_task_runner_)) {}

ZipArchiverSandboxSetupHooks::~ZipArchiverSandboxSetupHooks() = default;

ResultCode ZipArchiverSandboxSetupHooks::UpdateSandboxPolicy(
    sandbox::TargetPolicy* policy,
    base::CommandLine* command_line) {
  DCHECK(policy);
  DCHECK(command_line);

  // Unretained pointer of |zip_archiver_ptr_| is safe because its deleter is
  // run on the same task runner. So it won't be deleted before this task.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(BindZipArchiverRemote,
                                base::Unretained(zip_archiver_.get()),
                                SetupSandboxMessagePipe(policy, command_line),
                                std::move(connection_error_handler_)));

  return RESULT_CODE_SUCCESS;
}

RemoteZipArchiverPtr ZipArchiverSandboxSetupHooks::TakeZipArchiverRemote() {
  return std::move(zip_archiver_);
}

}  // namespace chrome_cleaner
