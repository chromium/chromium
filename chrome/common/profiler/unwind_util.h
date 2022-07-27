// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
#define CHROME_COMMON_PROFILER_UNWIND_UTIL_H_

#include "base/profiler/stack_sampling_profiler.h"

// A helper class to encapsulate some functionality related to stack unwinding.
class UnwindPrerequisites {
 public:
  // Request the installation of any prerequisites needed for unwinding.
  // Android, in particular, requires use of a dynamic feature module to provide
  // the native unwinder.
  //
  // Note that installation of some prerequisites can occur asynchronously.
  // Therefore, it's not guaranteed that Available() will return true
  // immediately after calling RequestInstallation().
  //
  // RequestInstallation() can only be called from the browser process.
  static void RequestInstallation();

  // Are the prerequisites required for unwinding available in the current
  // context?
  static bool Available();
};

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory();

#endif  // CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
