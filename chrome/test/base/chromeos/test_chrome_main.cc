// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/test/base/chromeos/test_chrome_base.h"

extern "C" {
__attribute__((visibility("default"))) int ChromeMain(int argc,
                                                      const char** argv);
}

int ChromeMain(int argc, const char** argv) {
  ChromeMainDelegate chrome_main_delegate(base::TimeTicks::Now());
  content::ContentMainParams params(&chrome_main_delegate);

  params.argc = argc;
  params.argv = argv;
  base::CommandLine::Init(params.argc, params.argv);

  // Start the sampling profiler as early as possible - namely, once the command
  // line data is available. Allocated as an object on the stack to ensure that
  // the destructor runs on shutdown, which is important to avoid the profiler
  // thread's destruction racing with main thread destruction.
  // This is the same as
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/app/chrome_main.cc?q=scoped_sampling_profiler
  // The way it's currently implemented requires us to keep it, but from a
  // technical perspective it shouldn't be necessary. There's no need to
  // run the sampling profiler for test ash chrome.
  MainThreadStackSamplingProfiler scoped_sampling_profiler;

  test::TestChromeBase test_chrome_base(std::move(params));
  return test_chrome_base.Start();
}
