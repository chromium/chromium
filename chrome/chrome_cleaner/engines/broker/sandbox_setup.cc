// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/sandbox_setup.h"

#include <windows.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "chrome/chrome_cleaner/engines/target/engine_commands_impl.h"  // nogncheck
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"  // nogncheck
#include "chrome/chrome_cleaner/engines/target/engine_delegate_factory.h"  // nogncheck
#include "chrome/chrome_cleaner/engines/target/libraries.h"  // nogncheck
#include "mojo/public/cpp/bindings/interface_request.h"
#endif

namespace chrome_cleaner {

namespace {

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
ResultCode SpawnWithoutSandboxForTesting(
    Engine::Name engine_name,
    scoped_refptr<EngineClient> engine_client,
    scoped_refptr<chrome_cleaner::MojoTaskRunner> mojo_task_runner) {
  // Extract the libraries to the same directory as the executable. When using
  // a sandbox this is done in RunEngineSandboxTarget.
  base::FilePath extraction_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &extraction_dir));
  if (!LoadAndValidateLibraries(engine_name, extraction_dir)) {
    NOTREACHED() << "Binary signature validation failed";
    return RESULT_CODE_SIGNATURE_VERIFICATION_FAILED;
  }

  // EngineCommandsImpl must be created on the mojo thread. Create one that
  // leaks deliberately since this is only for testing and it needs to outlive
  // the EngineClient object.
  //
  // When using a sandbox, the remote is bound to the broker end of a pipe in
  // EngineClient::PostBindEngineCommandsRemote and the impl is bound to the
  // target end in EngineMojoSandboxTargetHooks::BindEngineCommandsReceiver.
  // This binds the remote directly to the impl. There's no need for an error
  // handling callback because there's no pipe that can have errors.
  mojo_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<EngineClient> engine_client,
             scoped_refptr<EngineDelegate> engine_delegate,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
            new EngineCommandsImpl(engine_delegate,
                                   engine_client->engine_commands_remote()
                                       ->BindNewPipeAndPassReceiver(),
                                   task_runner, base::DoNothing::Repeatedly());
          },
          engine_client, CreateEngineDelegate(engine_name), mojo_task_runner));

  // When using a sandbox this is done in
  // EngineSandboxSetupHooks::TargetResumed.
  uint32_t engine_result = engine_client->Initialize();
  if (engine_result != EngineResultCode::kSuccess) {
    LOG(DFATAL) << "Engine initialize failed with 0x" << std::hex
                << engine_result << std::dec;
    return RESULT_CODE_ENGINE_INITIALIZATION_FAILED;
  }

  return RESULT_CODE_SUCCESS;
}
#endif

}  // namespace
EngineSandboxSetupHooks::EngineSandboxSetupHooks(
    scoped_refptr<EngineClient> engine_client)
    : engine_client_(engine_client) {}

EngineSandboxSetupHooks::~EngineSandboxSetupHooks() = default;

ResultCode EngineSandboxSetupHooks::UpdateSandboxPolicy(
    sandbox::TargetPolicy* policy,
    base::CommandLine* command_line) {
  // Create a Mojo message pipe to talk to the sandbox target process.
  mojo::ScopedMessagePipeHandle mojo_pipe =
      SetupSandboxMessagePipe(policy, command_line);

  engine_client_->PostBindEngineCommandsRemote(std::move(mojo_pipe));

  // Propagate engine selection switches to the sandbox target.
  command_line->AppendSwitchNative(
      kEngineSwitch, base::NumberToString16(Settings::GetInstance()->engine()));

  return RESULT_CODE_SUCCESS;
}

ResultCode EngineSandboxSetupHooks::TargetResumed() {
  DCHECK(engine_client_);
  uint32_t engine_result = engine_client_->Initialize();
  if (engine_result != EngineResultCode::kSuccess) {
    LOG(DFATAL) << "Engine initialize failed with 0x" << std::hex
                << engine_result << std::dec;
    return RESULT_CODE_ENGINE_INITIALIZATION_FAILED;
  }
  return RESULT_CODE_SUCCESS;
}

std::pair<ResultCode, scoped_refptr<EngineClient>> SpawnEngineSandbox(
    Engine::Name engine_name,
    RegistryLogger* const registry_logger,
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    std::unique_ptr<InterfaceMetadataObserver> interface_metadata_observer) {
  scoped_refptr<EngineClient> engine_client = EngineClient::CreateEngineClient(
      engine_name,
      base::BindRepeating(&RegistryLogger::WriteExperimentalEngineResultCode,
                          base::Unretained(registry_logger)),
      connection_error_callback, mojo_task_runner,
      std::move(interface_metadata_observer));

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  if (chrome_cleaner::Settings::GetInstance()
          ->run_without_sandbox_for_testing()) {
    ResultCode result_code = SpawnWithoutSandboxForTesting(
        engine_name, engine_client, mojo_task_runner);
    return std::make_pair(result_code, engine_client);
  }
#endif

  EngineSandboxSetupHooks mojo_setup_hooks(engine_client.get());
  ResultCode result_code =
      SpawnSandbox(&mojo_setup_hooks, SandboxType::kEngine);
  return std::make_pair(result_code, engine_client);
}

}  // namespace chrome_cleaner
