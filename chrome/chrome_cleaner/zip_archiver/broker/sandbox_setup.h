// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ZIP_ARCHIVER_BROKER_SANDBOX_SETUP_H_
#define CHROME_CHROME_CLEANER_ZIP_ARCHIVER_BROKER_SANDBOX_SETUP_H_

#include <memory>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace chrome_cleaner {

using RemoteZipArchiverPtr = std::unique_ptr<mojo::Remote<mojom::ZipArchiver>,
                                             base::OnTaskRunnerDeleter>;

class ZipArchiverSandboxSetupHooks : public MojoSandboxSetupHooks {
 public:
  ZipArchiverSandboxSetupHooks(scoped_refptr<MojoTaskRunner> mojo_task_runner,
                               base::OnceClosure connection_error_handler);
  ~ZipArchiverSandboxSetupHooks() override;

  // SandboxSetupHooks

  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override;

  RemoteZipArchiverPtr TakeZipArchiverRemote();

 private:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  base::OnceClosure connection_error_handler_;
  RemoteZipArchiverPtr zip_archiver_;

  DISALLOW_COPY_AND_ASSIGN(ZipArchiverSandboxSetupHooks);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ZIP_ARCHIVER_BROKER_SANDBOX_SETUP_H_
