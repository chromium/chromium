// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
#define CHROME_COMMON_PROFILER_UNWIND_UTIL_H_

#include "base/profiler/stack_sampling_profiler.h"
#include "components/version_info/channel.h"

// See `RequestUnwindPrerequisitesInstallation` below for more context. Intended
// for unit testing.
class UnwindPrerequisitesDelegate {
 public:
  virtual ~UnwindPrerequisitesDelegate() = default;

  // This is not intended to be used directly, and instead should be used
  // through `RequestUnwindPrerequisitesInstallation` below.
  virtual void RequestInstallation(version_info::Channel channel) = 0;
};

// Request the installation of any prerequisites needed for unwinding.
//
// Whether installation is requested also depends on the specific Chrome
// channel.
//
// Note that installation of some prerequisites can occur asynchronously.
// Therefore, it's not guaranteed that AreUnwindPrerequisitesAvailable() will
// return true immediately after calling
// RequestUnwindPrerequisitesInstallation().
//
// RequestUnwindPrerequisitesInstallation() can only be called from the browser
// process.
//
// If `delegate` is provided, it is used to request installation of unwind
// prerequisites, on certain Android platforms only. Intended for unit testing.
void RequestUnwindPrerequisitesInstallation(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* delegate = nullptr);

// Are the prerequisites required for unwinding available in the current
// context?
bool AreUnwindPrerequisitesAvailable();

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory();

base::StackSamplingProfiler::UnwindersFactory
CreateLibunwindstackUnwinderFactory();

#endif  // CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
