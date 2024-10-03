// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/install_from_archive.h"

#import <Cocoa/Cocoa.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/util.h"

namespace updater {
namespace {

constexpr int kPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                 base::FILE_PERMISSION_GROUP_MASK |
                                 base::FILE_PERMISSION_READ_BY_OTHERS |
                                 base::FILE_PERMISSION_EXECUTE_BY_OTHERS;

bool RunHDIUtil(const std::vector<std::string>& args,
                std::string* command_output) {
  base::FilePath hdiutil_path("/usr/bin/hdiutil");
  if (!base::PathExists(hdiutil_path)) {
    VLOG(1) << "hdiutil path (" << hdiutil_path << ") does not exist.";
    return false;
  }

  base::CommandLine command(hdiutil_path);
  for (const auto& arg : args) {
    command.AppendArg(arg);
  }

  std::string output;
  bool result = base::GetAppOutput(command, &output);
  if (!result) {
    VLOG(1) << "hdiutil failed.";
  }

  if (command_output) {
    *command_output = output;
  }

  return result;
}

bool MountDMG(const base::FilePath& dmg_path, std::string* mount_point) {
  if (!base::PathExists(dmg_path)) {
    VLOG(1) << "The DMG file path (" << dmg_path << ") does not exist.";
    return false;
  }

  std::string command_output;
  std::vector<std::string> args{"attach", dmg_path.value(), "-plist",
                                "-nobrowse", "-readonly"};
  if (!RunHDIUtil(args, &command_output)) {
    VLOG(1) << "Mounting DMG (" << dmg_path
            << ") failed. Output: " << command_output;
    return false;
  }
  @autoreleasepool {
    NSDictionary* plist = nil;
    @try {
      plist = [base::SysUTF8ToNSString(command_output) propertyList];
    } @catch (NSException*) {
      // `[NSString propertyList]` throws an NSParseErrorException if bad data.
      VLOG(1) << "Unable to parse command output: [" << command_output << "]";
      return false;
    }
    // Look for the mountpoint.
    NSArray* system_entities = [plist objectForKey:@"system-entities"];
    NSString* dmg_mount_point = nil;
    for (NSDictionary* entry in system_entities) {
      NSString* entry_mount_point = entry[@"mount-point"];
      if ([entry_mount_point length]) {
        dmg_mount_point = [entry_mount_point stringByStandardizingPath];
        break;
      }
    }
    if (mount_point) {
      *mount_point = base::SysNSStringToUTF8(dmg_mount_point);
    }
  }
  return true;
}

bool UnmountDMG(const base::FilePath& mounted_dmg_path) {
  if (!base::PathExists(mounted_dmg_path)) {
    VLOG(1) << "The mounted DMG path (" << mounted_dmg_path
            << ") does not exist.";
    return false;
  }

  std::vector<std::string> args{"detach", mounted_dmg_path.value(), "-force"};
  if (!RunHDIUtil(args, nullptr)) {
    VLOG(1) << "Unmounting DMG (" << mounted_dmg_path << ") failed.";
    return false;
  }
  return true;
}

bool IsInstallScriptExecutable(const base::FilePath& script_path) {
  int permissions = 0;
  if (!base::GetPosixFilePermissions(script_path, &permissions)) {
    return false;
  }

  constexpr int kExecutableMask = base::FILE_PERMISSION_EXECUTE_BY_USER;
  return (permissions & kExecutableMask) == kExecutableMask;
}

int RunExecutable(const base::FilePath& existence_checker_path,
                  const std::string& ap,
                  const std::string& arguments,
                  const std::optional<base::FilePath>& installer_data_file,
                  const UpdaterScope& scope,
                  const base::Version& pv,
                  bool usage_stats_enabled,
                  base::TimeDelta timeout,
                  const base::FilePath& unpacked_path) {
  if (!base::PathExists(unpacked_path)) {
    VLOG(1) << "File path (" << unpacked_path << ") does not exist.";
    return static_cast<int>(InstallErrors::kMountedDmgPathDoesNotExist);
  }
  int run_executables = 0;
  for (const char* executable : {
           ".preinstall",
           ".keystone_preinstall",
           ".install",
           ".keystone_install",
           ".postinstall",
           ".keystone_postinstall",
       }) {
    base::FilePath executable_file_path = unpacked_path.Append(executable);
    if (!base::PathExists(executable_file_path)) {
      continue;
    }

    if (!IsInstallScriptExecutable(executable_file_path)) {
      VLOG(1) << "Executable file path (" << executable_file_path
              << ") is not executable";
      return static_cast<int>(InstallErrors::kExecutablePathNotExecutable);
    }

    base::CommandLine command(executable_file_path);
    command.AppendArgPath(unpacked_path);
    command.AppendArgPath(existence_checker_path);
    command.AppendArg(pv.GetString());

    std::string env_path = "/bin:/usr/bin";
    std::optional<base::FilePath> ksadmin_path =
        GetKSAdminPath(GetUpdaterScope());
    if (ksadmin_path) {
      env_path = base::StrCat({env_path, ":", ksadmin_path->DirName().value()});
    }

    base::ScopedFD read_fd, write_fd;
    {
      int pipefds[2] = {};
      if (pipe(pipefds) != 0) {
        VPLOG(1) << "pipe";
        return static_cast<int>(InstallErrors::kExecutablePipeFailed);
      }
      read_fd.reset(pipefds[0]);
      write_fd.reset(pipefds[1]);
    }

    base::LaunchOptions options;
    options.fds_to_remap.emplace_back(write_fd.get(), STDOUT_FILENO);
    options.fds_to_remap.emplace_back(write_fd.get(), STDERR_FILENO);
    options.current_directory = unpacked_path;
    options.clear_environment = true;
    options.environment = {
        {"KS_TICKET_AP", ap},
        {"KS_TICKET_SERVER_URL", UPDATE_CHECK_URL},
        {"KS_TICKET_XC_PATH", existence_checker_path.value()},
        {"PATH", env_path},
        {"PREVIOUS_VERSION", pv.GetString()},
        {"SERVER_ARGS", arguments},
        {"UPDATE_IS_MACHINE", IsSystemInstall(scope) ? "1" : "0"},
        {"UNPACK_DIR", unpacked_path.value()},
        {kUsageStatsEnabled,
         usage_stats_enabled ? kUsageStatsEnabledValueEnabled : "0"},
    };
    if (installer_data_file) {
      options.environment.emplace(base::ToUpperASCII(kInstallerDataSwitch),
                                  installer_data_file->value());
    }

    int exit_code = 0;
    VLOG(1) << "Running " << command.GetCommandLineString();
    const base::Process proc = base::LaunchProcess(command, options);
    if (!proc.IsValid()) {
      return static_cast<int>(InstallErrors::kExecutableWaitForExitFailed);
    }

    // Close write_fd to generate EOF in the read loop below.
    write_fd.reset();

    std::string output;
    base::Time deadline = base::Time::Now() + timeout;

    constexpr size_t kBufferSize = 1024;
    base::CheckedNumeric<size_t> total_bytes_read = 0;
    ssize_t read_this_pass = 0;
    do {
      struct pollfd fds[1] = {{.fd = read_fd.get(), .events = POLLIN}};
      int timeout_remaining_ms =
          static_cast<int>((deadline - base::Time::Now()).InMilliseconds());
      if (timeout_remaining_ms < 0 || poll(fds, 1, timeout_remaining_ms) != 1) {
        break;
      }
      base::CheckedNumeric<size_t> new_size =
          base::CheckedNumeric<size_t>(output.size()) +
          base::CheckedNumeric<size_t>(kBufferSize);
      if (!new_size.IsValid() || !total_bytes_read.IsValid()) {
        // Ignore the rest of the output.
        break;
      }
      output.resize(new_size.ValueOrDie());
      read_this_pass = HANDLE_EINTR(read(
          read_fd.get(), &output[total_bytes_read.ValueOrDie()], kBufferSize));
      if (read_this_pass >= 0) {
        total_bytes_read += base::CheckedNumeric<size_t>(read_this_pass);
        if (!total_bytes_read.IsValid()) {
          // Ignore the rest of the output.
          break;
        }
        output.resize(total_bytes_read.ValueOrDie());
      }
    } while (read_this_pass > 0);

    VLOG(1) << "Output from " << executable << ": " << output;

    if (!proc.WaitForExitWithTimeout(deadline - base::Time::Now(),
                                     &exit_code)) {
      return static_cast<int>(InstallErrors::kExecutableWaitForExitFailed);
    }
    if (exit_code != 0) {
      return exit_code;
    }
    ++run_executables;
  }
  return run_executables > 0
             ? 0
             : static_cast<int>(InstallErrors::kExecutableFilePathDoesNotExist);
}

void CopyDMGContents(const base::FilePath& dmg_path,
                     const base::FilePath& destination_path) {
  base::FileEnumerator(
      dmg_path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES)
      .ForEach([&destination_path](const base::FilePath& path) {
        base::File::Info file_info;
        if (!base::GetFileInfo(path, &file_info)) {
          VLOG(0) << "Couldn't get file info for: " << path.value();
          return;
        }

        if (base::IsLink(path)) {
          VLOG(0) << "File is symbolic link: " << path.value();
          return;
        }

        if (file_info.is_directory) {
          if (!base::CopyDirectory(path, destination_path, true)) {
            VLOG(0) << "Couldn't copy directory for: " << path.value() << " to "
                    << destination_path.value();
            return;
          }
        } else {
          if (!base::CopyFile(path, destination_path.Append(path.BaseName()))) {
            VLOG(0) << "Couldn't copy file for: " << path.value() << " to "
                    << destination_path.value();
            return;
          }
        }
      });
}

// Mounts the DMG specified by `dmg_file_path`. The install executable located
// at "/.install" in the mounted volume is executed, and then the DMG is
// un-mounted. Returns an error code if mounting the DMG or executing the
// executable failed.
int InstallFromDMG(const base::FilePath& dmg_file_path,
                   base::OnceCallback<int(const base::FilePath&)> install) {
  std::string mount_point;
  if (!MountDMG(dmg_file_path, &mount_point)) {
    return static_cast<int>(InstallErrors::kFailMountDmg);
  }

  if (mount_point.empty()) {
    VLOG(1) << "No mount point.";
    return static_cast<int>(InstallErrors::kNoMountPoint);
  }
  const base::FilePath mounted_dmg_path = base::FilePath(mount_point);
  const int result = std::move(install).Run(mounted_dmg_path);

  // After running the executable, before unmount, copy the contents of the DMG
  // into the cache folder. This will allow for differentials.
  CopyDMGContents(mounted_dmg_path, dmg_file_path.DirName());

  if (!UnmountDMG(mounted_dmg_path)) {
    VLOG(1) << "Could not unmount the DMG: " << mounted_dmg_path;
  }

  // Delete the DMG from the cached folder after we are done.
  if (!base::DeleteFile(dmg_file_path)) {
    VPLOG(1) << "Couldn't remove the DMG.";
  }

  return result;
}

// Unzips the zip using the existing unzip utility in Mac. Path to the zip is
// specified by the `zip_file_path`. The install executable located at
// "/.install" in the contents of the zip is executed, and then the zip is
// deleted. Returns an error code if unzipping the archive or executing the
// executable failed.
int InstallFromZip(const base::FilePath& zip_file_path,
                   base::OnceCallback<int(const base::FilePath&)> install) {
  const base::FilePath dest_path = zip_file_path.DirName();

  if (!UnzipWithExe(zip_file_path, dest_path)) {
    VLOG(1) << "Failed to unzip zip file.";
    return static_cast<int>(InstallErrors::kFailedToExpandZip);
  }

  if (!ConfirmFilePermissions(dest_path, kPermissionsMask)) {
    return static_cast<int>(InstallErrors::kCouldNotConfirmAppPermissions);
  }

  const int result = std::move(install).Run(dest_path);

  // Remove the zip file, keep the expanded.
  base::DeleteFile(zip_file_path);

  return result;
}

// Installs with a path to the app specified by the `app_file_path`. The install
// executable located at "/.install" next to the .app is executed. This function
// is important for the differential installs, as applying the differential
// creates a .app file within the caching folder.
int InstallFromApp(const base::FilePath& app_file_path,
                   base::OnceCallback<int(const base::FilePath&)> install) {
  if (!base::PathExists(app_file_path) ||
      app_file_path.FinalExtension() != ".app") {
    VLOG(1) << "Path to the app does not exist!";
    return static_cast<int>(InstallErrors::kNotSupportedInstallerType);
  }

  // Need to make sure that the app at the path being installed has the correect
  // permissions.
  if (!ConfirmFilePermissions(app_file_path, kPermissionsMask)) {
    return static_cast<int>(InstallErrors::kCouldNotConfirmAppPermissions);
  }

  return std::move(install).Run(app_file_path.DirName());
}
}  // namespace

int InstallFromArchive(const base::FilePath& file_path,
                       const base::FilePath& existence_checker_path,
                       const std::string& ap,
                       const UpdaterScope& scope,
                       const base::Version& pv,
                       const std::string& arguments,
                       const std::optional<base::FilePath>& installer_data_file,
                       const bool usage_stats_enabled,
                       base::TimeDelta timeout) {
  const std::map<std::string,
                 int (*)(const base::FilePath&,
                         base::OnceCallback<int(const base::FilePath&)>)>
      handlers = {
          {".dmg", &InstallFromDMG},
          {".zip", &InstallFromZip},
          {".app", &InstallFromApp},
      };
  auto handler = handlers.find(file_path.Extension());
  if (handler == handlers.end()) {
    VLOG(0) << "Install failed: no handler for " << file_path.Extension();
    return static_cast<int>(InstallErrors::kNotSupportedInstallerType);
  }
  return handler->second(
      file_path, base::BindOnce(&RunExecutable, existence_checker_path, ap,
                                arguments, installer_data_file, scope, pv,
                                usage_stats_enabled, timeout));
}
}  // namespace updater
