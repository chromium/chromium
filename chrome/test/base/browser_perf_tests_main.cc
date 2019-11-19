// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Always run browser perf tests serially - parallel running would be less
  // deterministic and distort perf measurements.
  size_t parallel_jobs = 1U;

  ChromeTestSuiteRunner runner;
  ChromeTestLauncherDelegate delegate(&runner);
  return LaunchChromeTests(parallel_jobs, &delegate, argc, argv);
}
