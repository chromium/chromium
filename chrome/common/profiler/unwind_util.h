// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
#define CHROME_COMMON_PROFILER_UNWIND_UTIL_H_

#include "base/profiler/stack_sampling_profiler.h"

// Request the installation of any prerequisites needed for unwinding.
// Android, in particular, requires use of a dynamic feature module to provide
// the native unwinder.
//
// Note that installation of some prerequisites can occur asynchronously.
// Therefore, it's not guaranteed that AreUnwindPrerequisitesAvailable() will
// return true immediately after calling
// RequestUnwindPrerequisitesInstallation().
//
// RequestUnwindPrerequisitesInstallation() can only be called from the browser
// process.
void RequestUnwindPrerequisitesInstallation();

// Are the prerequisites required for unwinding available in the current
// context?
bool AreUnwindPrerequisitesAvailable();

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory();

#endif  // CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
