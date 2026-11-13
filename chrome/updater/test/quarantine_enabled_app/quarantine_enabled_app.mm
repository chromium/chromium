// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs the command in its arguments, which inherits file quarantine from this
// app's LSFileQuarantineEnabled, as the updater does from the browser.

#import <AppKit/AppKit.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "base/process/launch.h"
#include "base/process/process.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    return EXIT_FAILURE;
  }
  @autoreleasepool {
    // LaunchServices applies LSFileQuarantineEnabled when the app checks in.
    [NSApplication sharedApplication];
  }
  base::Process process = base::LaunchProcess(
      std::vector<std::string>(argv + 1, argv + argc), base::LaunchOptions());
  int exit_code = EXIT_FAILURE;
  if (!process.IsValid() || !process.WaitForExit(&exit_code)) {
    return EXIT_FAILURE;
  }
  return exit_code;
}
