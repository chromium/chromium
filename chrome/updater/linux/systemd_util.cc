// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/linux/systemd_util.h"

#include <fcntl.h>
#include <sys/un.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/posix_util.h"

namespace updater {
// Allows the utility functions below to join processes. To avoid overzealously
// granting access to |base::ScopedAllowBaseSyncPrimitives|, this class must
// continue to live in a `.cc`.
class [[maybe_unused, nodiscard]] SystemctlLauncherScopedAllowBaseSyncPrimitives
    : public base::ScopedAllowBaseSyncPrimitives {};

namespace {
// Location of system-scoped unit files.
const base::FilePath kSystemUnitDirectory("/etc/systemd/system");
// Location of user-scoped unit files relative to the user's home directory.
const base::FilePath kUserUnitRelativeDirectory(".local/share/systemd/user");
// Systemd unit names.
constexpr char kUpdaterServiceName[] = PRODUCT_FULLNAME_STRING ".service";
constexpr char kUpdaterSocketName[] = PRODUCT_FULLNAME_STRING ".socket";
// Systemd unit definition templates.
constexpr char kUpdaterServiceDefinitionTemplate[] =
    "[Service]\n"
    "ExecStart=%s\n"
    "KillMode=process";  // Ensure systemd does not kill child processes when
                         // the main process exits.
constexpr char kUpdaterSocketDefinitionTemplate[] =
    "[Socket]\n"
    "ListenStream=%s\n"
    "\n"
    "[Install]\n"
    "WantedBy=sockets.target";

// Returns the path to the systemd unit directory for the given scope.
std::optional<base::FilePath> GetUnitDirectory(UpdaterScope scope) {
  base::FilePath unit_dir;
  switch (scope) {
    case UpdaterScope::kUser:
      if (!base::PathService::Get(base::DIR_HOME, &unit_dir)) {
        return std::nullopt;
      }
      unit_dir = unit_dir.Append(kUserUnitRelativeDirectory);

      if (!base::CreateDirectory(unit_dir)) {
        return std::nullopt;
      }
      break;
    case UpdaterScope::kSystem:
      unit_dir = base::FilePath(kSystemUnitDirectory);
  }
  return unit_dir;
}

// Returns the command `systemctl` with or without the `--user` flag.
base::CommandLine GetBaseSystemctlCommand(UpdaterScope scope) {
  base::CommandLine command({"systemctl"});
  if (scope == UpdaterScope::kUser) {
    command.AppendSwitch("user");
  }
  return command;
}

// Launch the given command line with stdin, stdout, and stderr remapped to
// /dev/null.
void LaunchWithRemaps(base::CommandLine command) {
  base::LaunchOptions options;

  base::ScopedFD null_fd(HANDLE_EINTR(open("/dev/null", O_RDWR)));
  if (null_fd.is_valid()) {
    options.fds_to_remap.emplace_back(null_fd.get(), STDIN_FILENO);
  }

  base::Process proc = base::LaunchProcess(command, options);
  if (!proc.IsValid()) {
    VLOG(1) << "Could not launch " << command.GetCommandLineString();
  } else {
    SystemctlLauncherScopedAllowBaseSyncPrimitives allow_wait;
    proc.WaitForExit(nullptr);
  }
}

// Runs the daemon-reload command to have systemd reload unit files.
void ReloadUnitFiles(UpdaterScope scope) {
  base::CommandLine command = GetBaseSystemctlCommand(scope);
  command.AppendArg("daemon-reload");
  LaunchWithRemaps(command);
}

// Enables the socket unit to be created and managed by systemd at boot.
void EnableSocketUnit(UpdaterScope scope) {
  base::CommandLine command = GetBaseSystemctlCommand(scope);
  command.AppendArg("enable");
  command.AppendSwitch("now");
  command.AppendArg(kUpdaterSocketName);

  LaunchWithRemaps(command);
}

// Ensures that the update service and socket are stopped.
void StopService(UpdaterScope scope) {
  base::CommandLine command = GetBaseSystemctlCommand(scope);
  command.AppendArg("stop");
  command.AppendArg(kUpdaterServiceName);
  command.AppendArg(kUpdaterSocketName);
  LaunchWithRemaps(command);
}

// Writes the contents of |unit_definition| to |unit_path|. Returns true on
// success.
[[nodiscard]] bool InstallSystemdUnit(base::FilePath unit_path,
                                      std::string unit_definition) {
  base::File unit_file(unit_path,
                       base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  return unit_file.IsValid() && unit_file.Write(0, unit_definition.c_str(),
                                                unit_definition.size()) != -1;
}

// Returns the command line which should be used to start the updater service.
std::string GetLauncherCommandLine(UpdaterScope scope,
                                   base::FilePath launcher) {
  base::CommandLine command(launcher);
  command.AppendSwitch(kServerSwitch);
  command.AppendSwitchASCII(kServerServiceSwitch,
                            kServerUpdateServiceSwitchValue);
  if (scope == UpdaterScope::kSystem) {
    command.AppendSwitch(kSystemSwitch);
  }
  return command.GetCommandLineString();
}

}  // namespace

SystemdService::SystemdService() {
  if (sd_listen_fds(0) == 1) {
    server_socket_ = base::ScopedFD(SD_LISTEN_FDS_START + 0);
    read_watcher_controller_ = base::FileDescriptorWatcher::WatchReadable(
        server_socket_.get(),
        base::BindRepeating(&SystemdService::OnSocketReadable,
                            weak_factory_.GetWeakPtr()));
  }
}

SystemdService::~SystemdService() = default;

void SystemdService::OnSocketReadable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(server_socket_.is_valid());

  base::ScopedFD remote_fd(accept(server_socket_.get(), nullptr, nullptr));
  if (!remote_fd.is_valid()) {
    VPLOG(1) << "Failed to accept connection on activation socket.";
  }
}

bool InstallSystemdUnits(UpdaterScope scope) {
  std::optional<base::FilePath> launcher_path =
      GetUpdateServiceLauncherPath(scope);
  std::optional<base::FilePath> unit_dir = GetUnitDirectory(scope);
  if (!launcher_path || !unit_dir) {
    return false;
  }

  // Uninstall existing units if they exist.
  UninstallSystemdUnits(scope);

  if (!InstallSystemdUnit(
          unit_dir->AppendASCII(kUpdaterServiceName),
          base::StringPrintf(
              kUpdaterServiceDefinitionTemplate,
              GetLauncherCommandLine(scope, *launcher_path).c_str())) ||
      !InstallSystemdUnit(
          unit_dir->AppendASCII(kUpdaterSocketName),
          base::StringPrintf(
              kUpdaterSocketDefinitionTemplate,
              GetActivationSocketPath(scope).AsUTF8Unsafe().c_str()))) {
    // Avoid a partial installation.
    UninstallSystemdUnits(scope);
    return false;
  }

  ReloadUnitFiles(scope);
  EnableSocketUnit(scope);
  LOG_IF(ERROR, !base::PathExists(GetActivationSocketPath(scope)))
      << "Activation socket file is missing post-install.";
  return true;
}

bool UninstallSystemdUnits(UpdaterScope scope) {
  std::optional<base::FilePath> unit_dir = GetUnitDirectory(scope);
  if (!unit_dir) {
    return false;
  }

  StopService(scope);

  // Note: It is considered successful to attempt to delete units that do not
  // exist.
  bool success = base::DeleteFile(unit_dir->AppendASCII(kUpdaterServiceName)) &&
                 base::DeleteFile(unit_dir->AppendASCII(kUpdaterSocketName));
  ReloadUnitFiles(scope);
  return success;
}

bool SystemdUnitsInstalled(UpdaterScope scope) {
  std::optional<base::FilePath> unit_dir = GetUnitDirectory(scope);
  if (!unit_dir) {
    return false;
  }

  return base::PathExists(unit_dir->AppendASCII(kUpdaterServiceName)) ||
         base::PathExists(unit_dir->AppendASCII(kUpdaterSocketName));
}

}  // namespace updater
