// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/test_app.h"

#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/update_client.h"
#include "chrome/updater/util.h"

namespace updater {

namespace {

class TestApp : public App {
 private:
  ~TestApp() override = default;
  void FirstTaskRun() override;

  void DoForegroundUpdate();
  void HandleCommandLine();
  void Register();
  void SetUpdateStatus(UpdateStatus status,
                       int progress,
                       bool rollback,
                       const std::string& version,
                       int64_t size,
                       const std::u16string& message);
};

void TestApp::SetUpdateStatus(UpdateStatus status,
                              int progress,
                              bool rollback,
                              const std::string& version,
                              int64_t size,
                              const std::u16string& message) {
  switch (status) {
    case UpdateStatus::INIT:
      VLOG(1) << "Updates starting!";
      break;
    case UpdateStatus::CHECKING:
      VLOG(1) << "Checking for updates...";
      break;
    case UpdateStatus::UPDATING:
      VLOG(1) << "Updating. Progress: " << progress;
      break;
    case UpdateStatus::UPDATED:
      VLOG(1) << "Current version is up to date.";
      Shutdown(0);
      break;
    case UpdateStatus::NEARLY_UPDATED:
      VLOG(1) << "Nearly updated. Needs restart.";
      Shutdown(0);
      break;
    case UpdateStatus::FAILED:
      LOG(ERROR) << "Update failed.";
      if (!message.empty())
        LOG(ERROR) << message;
      Shutdown(-1);
      break;
    default:
      NOTREACHED();
  }
}

void TestApp::Register() {
  UpdateClient::Create()->Register(base::BindOnce(&TestApp::Shutdown, this));
}

void TestApp::DoForegroundUpdate() {
  UpdateClient::Create()->CheckForUpdate(
      base::BindRepeating(&TestApp::SetUpdateStatus, this));
}

void TestApp::HandleCommandLine() {
  static constexpr base::TaskTraits kTaskTraitsBlockWithSyncPrimitives = {
      base::MayBlock(), base::WithBaseSyncPrimitives(),
      base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kInstallUpdaterSwitch)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, kTaskTraitsBlockWithSyncPrimitives,
        base::BindOnce(&InstallUpdater),
        base::BindOnce(&TestApp::Shutdown, this));
  } else if (command_line->HasSwitch(kRegisterToUpdaterSwitch)) {
    Register();
  } else if (command_line->HasSwitch(kForegroundUpdateSwitch)) {
    DoForegroundUpdate();
  } else if (command_line->HasSwitch(kRegisterUpdaterSwitch)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, kTaskTraitsBlockWithSyncPrimitives,
        base::BindOnce(&InstallUpdater),
        base::BindOnce(
            [](base::OnceClosure register_func,
               base::OnceCallback<void(int)> shutdown_func, int error) {
              if (error) {
                std::move(shutdown_func).Run(error);
                return;
              }
              std::move(register_func).Run();
            },
            base::BindOnce(&TestApp::Register, this),
            base::BindOnce(&TestApp::Shutdown, this)));
  } else {
    Shutdown(0);
  }
}

void TestApp::FirstTaskRun() {
  HandleCommandLine();
}

scoped_refptr<App> MakeTestApp() {
  return base::MakeRefCounted<TestApp>();
}

}  // namespace

int TestAppMain(int argc, const char** argv) {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  updater::InitLogging(FILE_PATH_LITERAL("test_app.log"));

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  return MakeTestApp()->Run();
}

}  // namespace updater
