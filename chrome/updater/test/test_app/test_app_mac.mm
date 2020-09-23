// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/test_app.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"

namespace updater {

namespace {

base::FilePath GetUpdaterAppName() {
  return base::FilePath(UPDATER_APP_FULLNAME_STRING ".app");
}

base::FilePath GetTestAppFrameworkName() {
  return base::FilePath(TEST_APP_FULLNAME_STRING " Framework.framework");
}

}  // namespace

int InstallUpdater() {
  // The updater executable is in
  // C.app/Contents/Frameworks/C.framework/Versions/V/Helpers/CUpdater.app.
  base::FilePath updater_bundle_path =
      base::mac::OuterBundlePath()
          .Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Frameworks"))
          .Append(FILE_PATH_LITERAL(GetTestAppFrameworkName()))
          .Append(FILE_PATH_LITERAL("Versions"))
          .Append(FILE_PATH_LITERAL(TEST_APP_VERSION_STRING))
          .Append(FILE_PATH_LITERAL("Helpers"))
          .Append(GetUpdaterAppName());

  if (!base::PathExists(updater_bundle_path)) {
    LOG(ERROR) << "Path to the updater app does not exist! path: "
               << updater_bundle_path;
    return -1;
  }

  base::FilePath updater_executable_path =
      updater_bundle_path.Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("MacOS"))
          .Append(FILE_PATH_LITERAL(UPDATER_APP_FULLNAME_STRING));

  if (!base::PathExists(updater_executable_path)) {
    LOG(ERROR) << "Path to the updater app does not exist!";
    return -2;
  }

  base::CommandLine command(updater_executable_path);
  command.AppendSwitch(kInstallSwitch);
  command.AppendSwitchASCII("--vmodule", "*/updater/*=2");

  std::string output;
  int exit_code = 0;
  base::GetAppOutputWithExitCode(command, &output, &exit_code);

  if (exit_code != 0) {
    LOG(ERROR) << "Couldn't install the updater. Exit code: " << exit_code;
    return exit_code;
  }

  return 0;
}

}  // namespace updater
