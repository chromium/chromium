// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/wake_task.h"

#include <Foundation/Foundation.h>

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/util.h"

namespace updater {

namespace {

NSString* NSStringSessionType(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return @"System";
    case UpdaterScope::kUser:
      return @"Aqua";
  }
}

NSString* MakeProgramArgument(const char* argument) {
  return base::SysUTF8ToNSString(base::StrCat({"--", argument}));
}

NSString* MakeProgramArgumentWithValue(const char* argument,
                                       const char* value) {
  return base::SysUTF8ToNSString(base::StrCat({"--", argument, "=", value}));
}

std::optional<base::FilePath> GetWakeTaskTarget(UpdaterScope scope) {
  std::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  if (!install_dir) {
    return std::nullopt;
  }
  return install_dir->Append("Current").Append(GetExecutableRelativePath());
}

}  // namespace

NSDictionary* CreateWakeLaunchdPlist(UpdaterScope scope) {
  std::optional<base::FilePath> target = GetWakeTaskTarget(scope);
  if (!target) {
    return nil;
  }

  // See the man page for launchd.plist.
  // Explicitly logging switches are no longer necessary but are not removed
  // because updating the plist file could cause a popup on newer macOS. We
  // can remove the logging switches the next time when we have to change the
  // plist file.
  NSMutableArray<NSString*>* program_arguments =
      [NSMutableArray<NSString*> array];
  [program_arguments addObjectsFromArray:@[
    base::SysUTF8ToNSString(target->value()),
    MakeProgramArgument(kWakeAllSwitch),
    MakeProgramArgument(kEnableLoggingSwitch),
    MakeProgramArgumentWithValue(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue)
  ]];
  if (IsSystemInstall(scope)) {
    [program_arguments addObject:MakeProgramArgument(kSystemSwitch)];
  }

  NSDictionary<NSString*, id>* launchd_plist = @{
    @LAUNCH_JOBKEY_LABEL : base::SysUTF8ToNSString(GetWakeLaunchdName(scope)),
    @LAUNCH_JOBKEY_PROGRAMARGUMENTS : program_arguments,
    @LAUNCH_JOBKEY_STARTINTERVAL : @3600,
    @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : NSStringSessionType(scope)
  };

  return launchd_plist;
}

}  // namespace updater
