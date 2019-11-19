// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_SANDBOX_SETUP_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_SANDBOX_SETUP_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// Sets up mojo communication channel with the sandboxed engine.
class EngineSandboxSetupHooks : public MojoSandboxSetupHooks {
 public:
  explicit EngineSandboxSetupHooks(scoped_refptr<EngineClient> engine_client);
  ~EngineSandboxSetupHooks() override;

  // SandboxSetupHooks

  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override;
  ResultCode TargetResumed() override;

 private:
  scoped_refptr<EngineClient> engine_client_;

  DISALLOW_COPY_AND_ASSIGN(EngineSandboxSetupHooks);
};

std::pair<ResultCode, scoped_refptr<EngineClient>> SpawnEngineSandbox(
    Engine::Name engine_name,
    RegistryLogger* const registry_logger,
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    std::unique_ptr<InterfaceMetadataObserver> interface_metadata_observer);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_SANDBOX_SETUP_H_
