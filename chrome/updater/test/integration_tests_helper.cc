// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_scope.h"
#include "url/gurl.h"

namespace updater {
namespace test {
namespace {

constexpr char kAppId[] = "app_id";
constexpr int kSuccess = 0;
constexpr int kUnknownSwitch = 101;
constexpr int kMissingAppIdSwitch = 102;
constexpr int kMissingUrlSwitch = 103;
constexpr int kMissingExitCodeSwitch = 104;
constexpr int kBadExitCodeSwitch = 105;
constexpr int kMissingPathSwitch = 106;
constexpr int kMissingVersionParameter = 107;

class AppTestHelper : public App {
 private:
  ~AppTestHelper() override = default;
  void FirstTaskRun() override;
  void InitializeThreadPool() override;
};

void AppTestHelper::FirstTaskRun() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  if (command_line->HasSwitch("clean")) {
    task_runner->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&Clean, UpdaterScope::kSystem),
        base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
  } else if (command_line->HasSwitch("expect_clean")) {
    ExpectClean(UpdaterScope::kSystem);
    Shutdown(kSuccess);
  } else if (command_line->HasSwitch("install")) {
    task_runner->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&Install, UpdaterScope::kSystem),
        base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
  } else if (command_line->HasSwitch("expect_installed")) {
    task_runner->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&ExpectInstalled, UpdaterScope::kSystem),
        base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
  } else if (command_line->HasSwitch("uninstall")) {
    task_runner->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&Uninstall, UpdaterScope::kSystem),
        base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
  } else if (command_line->HasSwitch("expect_candidate_uninstalled")) {
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ExpectCandidateUninstalled, UpdaterScope::kSystem),
        base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
  } else if (command_line->HasSwitch("enter_test_mode")) {
    if (command_line->HasSwitch("url")) {
      GURL url(command_line->GetSwitchValueASCII("url"));
      EnterTestMode(url);
      Shutdown(kSuccess);
    } else {
      Shutdown(kMissingUrlSwitch);
    }
  } else if (command_line->HasSwitch("expect_active_updater")) {
    task_runner->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&ExpectActiveUpdater, UpdaterScope::kSystem),
        base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
  } else if (command_line->HasSwitch("expect_version_active")) {
    if (command_line->HasSwitch("version")) {
      task_runner->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&ExpectVersionActive,
                         command_line->GetSwitchValueASCII("version")),
          base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
    } else {
      Shutdown(kMissingVersionParameter);
    }
  } else if (command_line->HasSwitch("expect_version_not_active")) {
    if (command_line->HasSwitch("version")) {
      task_runner->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&ExpectVersionNotActive,
                         command_line->GetSwitchValueASCII("version")),
          base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
    } else {
      Shutdown(kMissingVersionParameter);
    }
  } else if (command_line->HasSwitch("run_wake")) {
    if (command_line->HasSwitch("exit_code")) {
      int exit_code = -1;
      if (base::StringToInt(command_line->GetSwitchValueASCII("exit_code"),
                            &exit_code)) {
        task_runner->PostTaskAndReply(
            FROM_HERE,
            base::BindOnce(&RunWake, UpdaterScope::kSystem, exit_code),
            base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
      } else {
        Shutdown(kBadExitCodeSwitch);
      }
    } else {
      Shutdown(kMissingExitCodeSwitch);
    }
  } else if (command_line->HasSwitch("setup_fake_updater_higher_version")) {
    SetupFakeUpdaterHigherVersion(UpdaterScope::kSystem);
    Shutdown(kSuccess);
  } else if (command_line->HasSwitch("setup_fake_updater_lower_version")) {
    SetupFakeUpdaterLowerVersion(UpdaterScope::kSystem);
    Shutdown(kSuccess);
  } else if (command_line->HasSwitch("set_fake_existence_checker_path")) {
    if (command_line->HasSwitch(kAppId)) {
      SetFakeExistenceCheckerPath(command_line->GetSwitchValueASCII(kAppId));
      Shutdown(kSuccess);
    } else {
      Shutdown(kMissingAppIdSwitch);
    }
  } else if (command_line->HasSwitch(
                 "expect_app_unregistered_existence_checker_path")) {
    if (command_line->HasSwitch(kAppId)) {
      ExpectAppUnregisteredExistenceCheckerPath(
          command_line->GetSwitchValueASCII(kAppId));
      Shutdown(kSuccess);
    } else {
      Shutdown(kMissingAppIdSwitch);
    }
  } else if (command_line->HasSwitch("print_log")) {
    task_runner->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&PrintLog, UpdaterScope::kSystem),
        base::BindOnce(&AppTestHelper::Shutdown, this, kSuccess));
  } else if (command_line->HasSwitch("copy_log")) {
    if (command_line->HasSwitch("path")) {
      const std::string path_string = command_line->GetSwitchValueASCII("path");
      base::FilePath path;
#if defined(OS_WIN)
      base::FilePath::StringType str;
      path = base::UTF8ToWide(path_string.c_str(), path_string.size(), &str)
                 ? base::FilePath(str)
                 : base::FilePath();
#else
      path = base::FilePath(path_string);
#endif  // OS_WIN
      CopyLog(path);
      Shutdown(kSuccess);
    } else {
      Shutdown(kMissingPathSwitch);
    }
  }
#if defined(OS_MAC)
  else if (command_line->HasSwitch("register_app")) {
    if (command_line->HasSwitch(kAppId)) {
      RegisterApp(command_line->GetSwitchValueASCII(kAppId));
      Shutdown(kSuccess);
    } else {
      Shutdown(kMissingAppIdSwitch);
    }
  } else if (command_line->HasSwitch("register_test_app")) {
    RegisterTestApp(UpdaterScope::kSystem);
    Shutdown(kSuccess);
  }
#endif
  else {
    VLOG(0) << "No supported switch provided. Command: "
            << command_line->GetCommandLineString();
    Shutdown(kUnknownSwitch);
  }
}

void AppTestHelper::InitializeThreadPool() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("test_helper");
}

scoped_refptr<App> MakeAppTestHelper() {
  return base::MakeRefCounted<AppTestHelper>();
}

int IntegrationTestsHelperMain(int argc, char** argv) {
  base::PlatformThread::SetName("IntegrationTestsHelperMain");
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  base::CommandLine::Init(argc, argv);

  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);

  return MakeAppTestHelper()->Run();
}

}  // namespace
}  // namespace test
}  // namespace updater

int main(int argc, char** argv) {
  return updater::test::IntegrationTestsHelperMain(argc, argv);
}
