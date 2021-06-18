// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
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

constexpr int kSuccess = 0;
constexpr int kUnknownSwitch = 101;
constexpr int kBadCommand = 102;

template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const std::string&, Args...)> callback) {
  return base::BindLambdaForTesting([=](Args... args) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(flag)) {
      return callback.Run(command_line->GetSwitchValueASCII(flag),
                          std::move(args)...);
    }
    LOG(ERROR) << "Missing switch: " << flag;
    return false;
  });
}

// Overload for int switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(int, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        int flag_int = -1;
        if (base::StringToInt(flag, &flag_int)) {
          return callback.Run(flag_int, std::move(args)...);
        }
        return false;
      }));
}

// Overload for GURL switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const GURL&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(GURL(flag), std::move(args)...);
      }));
}

// Overload for FilePath switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::FilePath&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(base::FilePath::FromUTF8Unsafe(flag),
                            std::move(args)...);
      }));
}

template <typename Arg, typename... RemainingArgs>
base::RepeatingCallback<bool(RemainingArgs...)> WithArg(
    Arg arg,
    base::RepeatingCallback<bool(Arg, RemainingArgs...)> callback) {
  return base::BindRepeating(callback, arg);
}

// Adapts the input callback to take a shutdown callback as the final parameter.
template <typename... Args>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)>
WithShutdown(base::RepeatingCallback<int(Args...)> callback) {
  return base::BindLambdaForTesting(
      [=](Args... args, base::OnceCallback<void(int)> shutdown) {
        std::move(shutdown).Run(callback.Run(args...));
        return true;
      });
}

// Short-named wrapper around BindOnce.
template <typename... Args, typename... ProvidedArgs>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)> Wrap(
    int (*func)(Args...),
    ProvidedArgs... provided_args) {
  return WithShutdown(base::BindRepeating(func, provided_args...));
}

// Overload of Wrap for functions that return void. (Returns kSuccess.)
template <typename... Args>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)> Wrap(
    void (*func)(Args...)) {
  return WithShutdown(base::BindLambdaForTesting([=](Args... args) {
    func(args...);
    return kSuccess;
  }));
}

// Helper to shorten lines below.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSystemScope(
    base::RepeatingCallback<bool(UpdaterScope, Args...)> callback) {
  return WithArg(UpdaterScope::kSystem, callback);
}

class AppTestHelper : public App {
 private:
  ~AppTestHelper() override = default;
  void FirstTaskRun() override;
  void InitializeThreadPool() override;
};

void AppTestHelper::FirstTaskRun() {
  std::map<std::string,
           base::RepeatingCallback<bool(base::OnceCallback<void(int)>)>>
      commands =
  {
    // To add additional commands, first Wrap a pointer to the target
    // function (which should be declared in integration_tests_impl.h), and
    // then use the With* helper functions to provide its arguments.
    {"clean", WithSystemScope(Wrap(&Clean))},
    {"enter_test_mode", WithSwitch("url", Wrap(&EnterTestMode))},
    {"expect_active_updater", WithSystemScope(Wrap(&ExpectActiveUpdater))},
    {"expect_app_unregistered_existence_checker_path",
     WithSwitch("app_id", Wrap(&ExpectAppUnregisteredExistenceCheckerPath))},
    {"expect_candidate_uninstalled",
     WithSystemScope(Wrap(&ExpectCandidateUninstalled))},
    {"expect_clean", WithSystemScope(Wrap(&ExpectClean))},
    {"expect_installed", WithSystemScope(Wrap(&ExpectInstalled))},
#if defined(OS_WIN)
    {"expect_interfaces_registered",
     WithSystemScope(Wrap(&ExpectInterfacesRegistered))},
#endif  // OS_WIN
    {"expect_version_active",
     WithSwitch("version", Wrap(&ExpectVersionActive))},
    {"expect_version_not_active",
     WithSwitch("version", Wrap(&ExpectVersionNotActive))},
    {"install", WithSystemScope(Wrap(&Install))},
    {"print_log", WithSystemScope(Wrap(&PrintLog))},
    {"run_wake", WithSwitch("exit_code", WithSystemScope(Wrap(&RunWake)))},
#if defined(OS_MAC)
    {"register_app", WithSwitch("app_id", Wrap(&RegisterApp))},
    {"register_test_app", WithSystemScope(Wrap(&RegisterTestApp))},
#endif  // defined(OS_MAC)
    {"set_existence_checker_path",
     WithSwitch("path", WithSwitch("app_id", Wrap(&SetExistenceCheckerPath)))},
    {"setup_fake_updater_higher_version",
     WithSystemScope(Wrap(&SetupFakeUpdaterHigherVersion))},
    {"setup_fake_updater_lower_version",
     WithSystemScope(Wrap(&SetupFakeUpdaterLowerVersion))},
    {"set_first_registration_counter",
     WithSwitch("value", Wrap(&SetServerStarts))},
    {"uninstall", WithSystemScope(Wrap(&Uninstall))},
  };

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  for (const auto& entry : commands) {
    if (command_line->HasSwitch(entry.first)) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!entry.second.Run(base::BindOnce(&AppTestHelper::Shutdown, this))) {
        Shutdown(kBadCommand);
      }
      return;
    }
  }

  LOG(ERROR) << "No supported switch provided. Command: "
             << command_line->GetCommandLineString();
  Shutdown(kUnknownSwitch);
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
