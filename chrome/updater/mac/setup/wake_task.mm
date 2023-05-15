// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/wake_task.h"

#include <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

absl::optional<base::FilePath> GetWakeTaskTarget(UpdaterScope scope) {
  absl::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  if (!install_dir) {
    return absl::nullopt;
  }
  return install_dir->Append("Current").Append(GetExecutableRelativePath());
}

}  // namespace

NSDictionary* CreateWakeLaunchdPlist(UpdaterScope scope) {
  absl::optional<base::FilePath> target = GetWakeTaskTarget(scope);
  if (!target) {
    return nil;
  }

  // See the man page for launchd.plist.
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
    @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : NSStringSessionType(scope),
    @"AssociatedBundleIdentifiers" : @MAC_BUNDLE_IDENTIFIER_STRING
  };

  return launchd_plist;
}

}  // namespace updater
