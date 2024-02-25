// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/at_exit.h"
#include "base/threading/platform_thread.h"
#include "components/variations/cros_evaluate_seed/evaluate_seed.h"

// evaluate_seed reads the seed data from Local State and prints computed state,
// in a serialized format, to stdout.
// It does so with minimal dependencies, so that it can run *before* ash-chrome
// starts on ChromeOS, with minimal impact to image size and boot time.
int main(int argc, const char* argv[]) {
  base::PlatformThread::SetName("EvaluateSeedMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  return variations::cros_early_boot::evaluate_seed::EvaluateSeedMain(stdin,
                                                                      stdout);
}
