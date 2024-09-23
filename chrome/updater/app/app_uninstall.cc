// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall.h"

#include <optional>
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
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/win/setup/uninstall.h"
#elif BUILDFLAG(IS_POSIX)
#include "chrome/updater/posix/setup.h"
#endif

namespace updater {

std::vector<base::FilePath> GetVersionExecutablePaths(UpdaterScope scope) {
  const std::optional<base::FilePath> updater_folder_path =
      GetInstallDirectory(scope);
  if (!updater_folder_path) {
    LOG(ERROR) << __func__ << ": failed to get the updater install directory.";
    return {};
  }
  std::vector<base::FilePath> version_executable_paths;
  base::FileEnumerator(*updater_folder_path, false,
                       base::FileEnumerator::DIRECTORIES)
      .ForEach([&scope, &version_executable_paths](
                   const base::FilePath& version_folder_path) {
        // Skip the current version.
        if (version_folder_path == GetVersionedInstallDirectory(scope)) {
          return;
        }

        // Skip if the folder is not named as a valid version. All updater
        // version directories are named as valid versions.
        if (!base::Version(version_folder_path.BaseName().MaybeAsASCII())
                 .IsValid()) {
          return;
        }

        const base::FilePath version_executable_path =
            version_folder_path.Append(GetExecutableRelativePath());
        version_executable_paths.push_back(version_executable_path);
        VLOG(1) << __func__ << " : added to version_executable_paths: "
                << version_executable_path;
      });

  return version_executable_paths;
}

base::CommandLine GetUninstallSelfCommandLine(
    UpdaterScope scope,
    const base::FilePath& executable_path) {
  base::CommandLine command_line(executable_path);
  command_line.AppendSwitch(kUninstallSelfSwitch);
  if (IsSystemInstall(scope)) {
    command_line.AppendSwitch(kSystemSwitch);
  }
  return command_line;
}

namespace {

// Uninstalls the enterprise companion app if it exists.
[[nodiscard]] int UninstallEnterpriseCompanionApp() {
  std::optional<base::FilePath> install_dir =
      enterprise_companion::GetInstallDirectory();
  if (!install_dir) {
    VLOG(1) << __func__ << ": Cannot get enterprise companion app "
            << "installation directory, skips the uninstall.";
    return kErrorOk;
  }

  base::CommandLine command_line(
      install_dir->AppendASCII(kCompanionAppExecutableName));
  if (!base::PathExists(command_line.GetProgram())) {
    VLOG(1) << __func__ << ": Companion app not found, skip the uninstall.";
    return kErrorOk;
  }
  command_line.AppendSwitch(kUninstallCompanionAppSwitch);
  int exit_code = -1;
  std::string output;
  if (!base::GetAppOutputWithExitCode(command_line, &output, &exit_code)) {
    return kErrorFailedToUninstallCompanionApp;
  }
  VLOG(1) << __func__ << ": Ran: " << command_line.GetCommandLineString()
          << ": " << output << ": " << exit_code;
  return exit_code == 0 ? kErrorOk : kErrorFailedToUninstallCompanionApp;
}

// Uninstalls all versions not matching the current version of the updater for
// the given `scope`.
[[nodiscard]] int UninstallOtherVersions(UpdaterScope scope) {
  bool has_error = false;
  for (const base::FilePath& version_executable_path :
       GetVersionExecutablePaths(scope)) {
    const base::CommandLine command_line(
        GetUninstallSelfCommandLine(scope, version_executable_path));
    if (!base::PathExists(command_line.GetProgram())) {
      VLOG(1)
          << __func__
          << ": Other version updater has no main binary, skip the uninstall.";
      return kErrorOk;
    }
    int exit_code = -1;
    std::string output;
    if (base::GetAppOutputWithExitCode(command_line, &output, &exit_code)) {
      VLOG(1) << __func__ << ": Ran: " << command_line.GetCommandLineString()
              << ": " << output << ": " << exit_code;
      if (exit_code != 0) {
        has_error = true;
      }
    } else {
      VLOG(1) << "Failed to run the command to uninstall other versions.";
      has_error = true;
    }
  }
  return has_error ? kErrorFailedToUninstallOtherVersion : kErrorOk;
}

void UninstallInThreadPool(UpdaterScope scope,
                           base::OnceCallback<void(int)> shutdown) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](UpdaterScope scope) {
            int error_code = kErrorOk;
            if (IsSystemInstall(scope)) {
              error_code = UninstallEnterpriseCompanionApp();
            }
            if (int result = UninstallOtherVersions(scope);
                result != kErrorOk) {
#if !BUILDFLAG(IS_LINUX)
              // TODO(crbug.com/366249606): Ignores the errors when uninstalls
              // the other versions, because currently older Linux updater on
              // CIPD exits with error `kErrorFailedToDeleteFolder`.
              error_code = result;
#endif
            }
            if (int result = Uninstall(scope); result != kErrorOk) {
              error_code = result;
            }
            return error_code;
          },
          scope),
      std::move(shutdown));
}

}  // namespace

// AppUninstall uninstalls the updater.
class AppUninstall : public App {
 public:
  AppUninstall() = default;

 private:
  ~AppUninstall() override = default;
  [[nodiscard]] int Initialize() override;
  void FirstTaskRun() override;

  void UninstallAll(int reason);

  // Inter-process lock taken by AppInstall, AppUninstall, and AppUpdate. May
  // be null if the setup lock wasn't acquired.
  std::unique_ptr<ScopedLock> setup_lock_;

  // These may be null if the global prefs lock wasn't acquired.
  scoped_refptr<GlobalPrefs> global_prefs_;
  scoped_refptr<Configurator> config_;
};

int AppUninstall::Initialize() {
  setup_lock_ =
      CreateScopedLock(kSetupMutex, updater_scope(), kWaitForSetupLock);
  global_prefs_ = CreateGlobalPrefs(updater_scope());
  if (global_prefs_) {
    config_ = base::MakeRefCounted<Configurator>(global_prefs_,
                                                 CreateExternalConstants());
  }
  return kErrorOk;
}

void AppUninstall::UninstallAll(int reason) {
  update_client::CrxComponent uninstall_data;
  uninstall_data.ap = config_->GetUpdaterPersistedData()->GetAP(kUpdaterAppId);
  uninstall_data.app_id = kUpdaterAppId;
  uninstall_data.brand =
      config_->GetUpdaterPersistedData()->GetBrandCode(kUpdaterAppId);
  uninstall_data.requires_network_encryption = false;
  uninstall_data.version =
      config_->GetUpdaterPersistedData()->GetProductVersion(kUpdaterAppId);
  if (!uninstall_data.version.IsValid()) {
    // In cases where there is no version in persisted data, fall back to the
    // currently-running version of the updater.
    uninstall_data.version = base::Version(kUpdaterVersion);
  }

  // If the terms of service have not been accepted, don't ping.
  if (config_->GetUpdaterPersistedData()->GetEulaRequired()) {
    UninstallInThreadPool(updater_scope(),
                          base::BindOnce(&AppUninstall::Shutdown, this));
    return;
  }

  // Otherwise, send an uninstall ping then uninstall.
  update_client::UpdateClientFactory(config_)->SendPing(
      uninstall_data,
      {.event_type = update_client::protocol_request::kEventUninstall,
       .result = 1,
       .error_code = 0,
       .extra_code1 = reason},
      base::BindOnce(
          [](base::OnceCallback<void(int)> shutdown, UpdaterScope scope,
             update_client::Error uninstall_ping_error) {
            VLOG_IF(1, uninstall_ping_error != update_client::Error::NONE)
                << "Uninstall ping failed: " << uninstall_ping_error;
            UninstallInThreadPool(scope, std::move(shutdown));
          },
          base::BindOnce(&AppUninstall::Shutdown, this), updater_scope()));
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
    UninstallAll(kUninstallPingReasonUninstalled);
    return;
  }

  if (command_line->HasSwitch(kUninstallIfUnusedSwitch)) {
    const bool had_apps = config_->GetUpdaterPersistedData()->GetHadApps();
    const bool should_uninstall =
        ShouldUninstall(config_->GetUpdaterPersistedData()->GetAppIds(),
                        global_prefs_->CountServerStarts(), had_apps);
    VLOG(1) << "ShouldUninstall returned: " << should_uninstall;
    if (should_uninstall) {
      UninstallAll(had_apps ? kUninstallPingReasonNoAppsRemain
                            : kUninstallPingReasonNeverHadApps);
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&AppUninstall::Shutdown, this, 0));
    }
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

scoped_refptr<App> MakeAppUninstall() {
  return base::MakeRefCounted<AppUninstall>();
}

}  // namespace updater
