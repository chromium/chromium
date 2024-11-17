// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/wake_task.h"

#include <Foundation/Foundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

// This is a change detector test. If the wake plist changes, macOS may notify
// the user that the updater has registered new background tasks. Therefore, to
// minimize user annoyance, do not change the plist unintentionally.
TEST(WakeTask, NotModified) {
  NSDictionary<NSString*, id>* expected;

  switch (GetUpdaterScopeForTesting()) {
    case UpdaterScope::kSystem:
      expected = @{
        @LAUNCH_JOBKEY_LABEL : base::SysUTF8ToNSString(
            MAC_BUNDLE_IDENTIFIER_STRING ".wake.system"),
        @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[
          base::SysUTF8ToNSString(base::StrCat(
              {GetInstallDirectory(GetUpdaterScopeForTesting())->value(),
               "/Current/" PRODUCT_FULLNAME_STRING, kExecutableSuffix,
               ".app/Contents/MacOS/" PRODUCT_FULLNAME_STRING,
               kExecutableSuffix})),
          @"--wake-all",
          @"--enable-logging",
          base::SysUTF8ToNSString(
              base::StrCat({"--vmodule=", kLoggingModuleSwitchValue})),
          @"--system",
        ],
        @LAUNCH_JOBKEY_STARTINTERVAL : @3600,
        @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
        @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"System"
      };
      break;
    case UpdaterScope::kUser:
      expected = @{
        @LAUNCH_JOBKEY_LABEL :
            base::SysUTF8ToNSString(MAC_BUNDLE_IDENTIFIER_STRING ".wake"),
        @LAUNCH_JOBKEY_PROGRAMARGUMENTS : @[
          base::SysUTF8ToNSString(base::StrCat(
              {GetInstallDirectory(GetUpdaterScopeForTesting())->value(),
               "/Current/" PRODUCT_FULLNAME_STRING, kExecutableSuffix,
               ".app/Contents/MacOS/" PRODUCT_FULLNAME_STRING,
               kExecutableSuffix})),
          @"--wake-all",
          @"--enable-logging",
          base::SysUTF8ToNSString(
              base::StrCat({"--vmodule=", kLoggingModuleSwitchValue})),
        ],
        @LAUNCH_JOBKEY_STARTINTERVAL : @3600,
        @LAUNCH_JOBKEY_ABANDONPROCESSGROUP : @YES,
        @LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE : @"Aqua"
      };
      break;
  }
  EXPECT_TRUE([expected
      isEqualToDictionary:CreateWakeLaunchdPlist(GetUpdaterScopeForTesting())]);
}

}  // namespace updater
