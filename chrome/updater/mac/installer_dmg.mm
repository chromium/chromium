// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/installer_dmg.h"

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"

namespace updater {

namespace {

bool RunHDIUtil(const std::vector<std::string>& args,
                std::string* command_output) {
  base::FilePath hdiutil_path("/usr/bin/hdiutil");
  if (!base::PathExists(hdiutil_path)) {
    DLOG(ERROR) << "hdiutil path (" << hdiutil_path << ") does not exist.";
    return false;
  }

  base::CommandLine command(hdiutil_path);
  for (const auto& arg : args)
    command.AppendArg(arg);

  std::string output;
  bool result = base::GetAppOutput(command, &output);
  if (!result)
    DLOG(ERROR) << "hdiutil failed.";

  if (command_output)
    *command_output = output;

  return result;
}

bool MountDMG(const base::FilePath& dmg_path, std::string* mount_point) {
  if (!base::PathExists(dmg_path)) {
    DLOG(ERROR) << "The DMG file path (" << dmg_path << ") does not exist.";
    return false;
  }

  std::string command_output;
  std::vector<std::string> args{"attach", dmg_path.value(), "-plist",
                                "-nobrowse", "-readonly"};
  if (!RunHDIUtil(args, &command_output)) {
    DLOG(ERROR) << "Mounting DMG (" << dmg_path
                << ") failed. Output: " << command_output;
    return false;
  }
  @autoreleasepool {
    NSString* output = base::SysUTF8ToNSString(command_output);
    NSDictionary* plist = [output propertyList];
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
    if (mount_point)
      *mount_point = base::SysNSStringToUTF8(dmg_mount_point);
  }
  return true;
}

bool UnmountDMG(const base::FilePath& mounted_dmg_path) {
  if (!base::PathExists(mounted_dmg_path)) {
    DLOG(ERROR) << "The mounted DMG path (" << mounted_dmg_path
                << ") does not exist.";
    return false;
  }

  std::vector<std::string> args{"detach", mounted_dmg_path.value(), "-force"};
  if (!RunHDIUtil(args, nullptr)) {
    DLOG(ERROR) << "Unmounting DMG (" << mounted_dmg_path << ") failed.";
    return false;
  }
  return true;
}

bool IsInstallScriptExecutable(const base::FilePath& script_path) {
  int permissions = 0;
  if (!base::GetPosixFilePermissions(script_path, &permissions))
    return false;

  constexpr int kExecutableMask = base::FILE_PERMISSION_EXECUTE_BY_USER;
  return (permissions & kExecutableMask) == kExecutableMask;
}

int RunExecutable(const base::FilePath& mounted_dmg_path,
                  const base::FilePath& existence_checker_path,
                  const base::FilePath::StringPieceType executable_name,
                  const std::string& arguments) {
  if (!base::PathExists(mounted_dmg_path)) {
    DLOG(ERROR) << "File path (" << mounted_dmg_path << ") does not exist.";
    return static_cast<int>(InstallErrors::kMountedDmgPathDoesNotExist);
  }
  base::FilePath executable_file_path =
      mounted_dmg_path.Append(executable_name);
  if (!base::PathExists(executable_file_path)) {
    DLOG(ERROR) << "Executable file path (" << executable_file_path
                << ") does not exist.";
    return static_cast<int>(InstallErrors::kExecutableFilePathDoesNotExist);
  }

  if (!IsInstallScriptExecutable(executable_file_path)) {
    DLOG(ERROR) << "Executable file path (" << executable_file_path
                << ") is not executable";
    return static_cast<int>(InstallErrors::kExecutablePathNotExecutable);
  }

  // TODO(copacitt): Improve the way we parse args for CommandLine object.
  // http://crbug.com/1056818
  base::CommandLine command(executable_file_path);
  command.AppendArgPath(mounted_dmg_path);
  if (!arguments.empty()) {
    base::CommandLine::StringVector argv =
        base::SplitString(arguments, base::kWhitespaceASCII,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    argv.insert(argv.begin(), existence_checker_path.value());
    argv.insert(argv.begin(), mounted_dmg_path.value());
    argv.insert(argv.begin(), executable_file_path.value());
    command = base::CommandLine(argv);
  }

  std::string output;
  int exit_code = 0;
  base::GetAppOutputWithExitCode(command, &output, &exit_code);

  return exit_code;
}

}  // namespace

int InstallFromDMG(const base::FilePath& dmg_file_path,
                   const base::FilePath& existence_checker_path,
                   const std::string& arguments) {
  std::string mount_point;
  if (!MountDMG(dmg_file_path, &mount_point))
    return static_cast<int>(InstallErrors::kFailMountDmg);

  if (mount_point.empty()) {
    DLOG(ERROR) << "No mount point.";
    return static_cast<int>(InstallErrors::kNoMountPoint);
  }
  const base::FilePath mounted_dmg_path = base::FilePath(mount_point);
  int result = RunExecutable(mounted_dmg_path, existence_checker_path,
                             ".install", arguments);

  if (!UnmountDMG(mounted_dmg_path))
    DLOG(WARNING) << "Could not unmount the DMG: " << mounted_dmg_path;

  return result;
}

}  // namespace updater
