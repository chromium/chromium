// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "chrome/install_static/test/scoped_install_details.h"

int main(int argc, char** argv) {
  // Ensure that the CommandLine instance honors the command line passed in
  // instead of the default behavior on Windows which is to use the shell32
  // CommandLineToArgvW API. The chrome_elf_unittests test suite should not
  // depend on user32 directly or indirectly (For the curious shell32 depends
  // on user32)
  base::CommandLine::InitUsingArgvForTesting(argc, argv);

  install_static::ScopedInstallDetails scoped_install_details;

  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
