// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/win/setup/uninstall.h"
#elif BUILDFLAG(IS_POSIX)
#include "chrome/updater/posix/setup.h"
#endif

namespace updater {
namespace {

// Uninstalls all versions not matching this version of the updater for the
// given `scope`.
void UninstallOtherVersions(UpdaterScope scope) {
  const absl::optional<base::FilePath> updater_folder_path =
      GetInstallDirectory(scope);
  if (!updater_folder_path) {
    LOG(ERROR) << "Failed to get updater folder path.";
    return;
  }
  base::FileEnumerator file_enumerator(*updater_folder_path, true,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_folder_path = file_enumerator.Next();
       !version_folder_path.empty() &&
       version_folder_path != GetVersionedInstallDirectory(scope);
       version_folder_path = file_enumerator.Next()) {
    const base::FilePath version_executable_path =
        version_folder_path.Append(GetExecutableRelativePath());

    if (base::PathExists(version_executable_path)) {
      base::CommandLine command_line(version_executable_path);
      command_line.AppendSwitch(kUninstallSelfSwitch);
      if (IsSystemInstall(scope))
        command_line.AppendSwitch(kSystemSwitch);
      command_line.AppendSwitch(kEnableLoggingSwitch);
      command_line.AppendSwitchASCII(kLoggingModuleSwitch,
                                     kLoggingModuleSwitchValue);
      int exit_code = -1;
      std::string output;
      base::GetAppOutputWithExitCode(command_line, &output, &exit_code);
    } else {
      VLOG(1) << base::CommandLine::ForCurrentProcess()->GetCommandLineString()
              << " : Path doesn't exist: " << version_executable_path;
    }
  }
}

}  // namespace

// AppUninstall uninstalls the updater.
class AppUninstall : public App {
 public:
  AppUninstall() = default;

 private:
  ~AppUninstall() override = default;
  void Initialize() override;
  void Uninitialize() override;
  void FirstTaskRun() override;

  void UninstallAll();

  // Inter-process lock taken by AppInstall, AppUninstall, and AppUpdate.
  std::unique_ptr<ScopedLock> setup_lock_;

  scoped_refptr<GlobalPrefs> global_prefs_;
};

void AppUninstall::Initialize() {
  setup_lock_ =
      ScopedLock::Create(kSetupMutex, updater_scope(), kWaitForSetupLock);

  global_prefs_ = CreateGlobalPrefs(updater_scope());
}

void AppUninstall::Uninitialize() {
  global_prefs_ = nullptr;
}

void AppUninstall::UninstallAll() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](UpdaterScope scope) {
            UninstallOtherVersions(scope);
            return Uninstall(scope);
          },
          updater_scope()),
      base::BindOnce(&AppUninstall::Shutdown, this));
}

void AppUninstall::FirstTaskRun() {
  if (WrongUser(updater_scope())) {
    VLOG(0) << "The current user is not compatible with the current scope.";
    Shutdown(kErrorWrongUser);
    return;
  }

  if (!setup_lock_) {
    VLOG(0) << "Failed to acquire setup mutex; shutting down.";
    Shutdown(kErrorFailedToLockSetupMutex);
    return;
  }

  if (!global_prefs_) {
    VLOG(0) << "Failed to acquire global prefs; shutting down.";
    Shutdown(kErrorFailedToLockPrefsMutex);
    return;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kUninstallSwitch)) {
    UninstallAll();
    return;
  }

  if (command_line->HasSwitch(kUninstallIfUnusedSwitch)) {
    auto persisted_data = base::MakeRefCounted<PersistedData>(
        updater_scope(), global_prefs_->GetPrefService());
    const bool should_uninstall = ShouldUninstall(
        persisted_data->GetAppIds(), global_prefs_->CountServerStarts(),
        persisted_data->GetHadApps());
    VLOG(1) << "ShouldUninstall returned: " << should_uninstall;
    if (should_uninstall) {
      UninstallAll();
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&AppUninstall::Shutdown, this, 0));
    }
    return;
  }

  NOTREACHED();
}

scoped_refptr<App> MakeAppUninstall() {
  return base::MakeRefCounted<AppUninstall>();
}

}  // namespace updater
