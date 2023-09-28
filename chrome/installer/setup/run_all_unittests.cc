// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/setup/setup_util_unittest.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // Handle the --adjust-thread-priority switch, which is used to test the
  // installer::AdjustThreadPriority() function in a subprocess.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAdjustThreadPriority)) {
    return DoThreadPriorityAdjustment();
  }

  // Register Chrome Path provider so that we can get test data dir.
  chrome::RegisterPathProvider();

  install_static::ScopedInstallDetails scoped_install_details;

  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
