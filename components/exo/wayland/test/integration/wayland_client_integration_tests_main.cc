// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::TestSuite test_suite(argc, argv);

  // Tests may not run to completion if we do not run them serially.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
