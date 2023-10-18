// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
#define CHROME_COMMON_PROFILER_UNWIND_UTIL_H_

#include "base/feature_list.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/version_info/channel.h"

// Used to gate unwind prerequisites' installation for some unit tests.
BASE_DECLARE_FEATURE(kInstallAndroidUnwindDfm);

// See `RequestUnwindPrerequisitesInstallation` and
// `AreUnwindPrerequisitesAvailable` below for more context. Intended for unit
// testing.
class UnwindPrerequisitesDelegate {
 public:
  virtual ~UnwindPrerequisitesDelegate() = default;

  // These are not intended to be used directly, and instead should be used
  // through `RequestUnwindPrerequisitesInstallation` and
  // `AreUnwindPrerequisitesAvailable` below.
  virtual void RequestInstallation(version_info::Channel channel) = 0;
  virtual bool AreAvailable(version_info::Channel channel) = 0;
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
// If `prerequites_delegate` is provided, it is used to request installation of
// unwind prerequisites, on certain Android platforms only. Intended for unit
// testing.
void RequestUnwindPrerequisitesInstallation(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* prerequites_delegate = nullptr);

// Are the prerequisites required for unwinding available in the current
// context?
//
// If `prerequites_delegate` is provided, it is used to check availability of
// unwind prerequisites, on certain Android platforms only. This is intended for
// unit testing so that tests can provide a mocked delegate, if needed.
bool AreUnwindPrerequisitesAvailable(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* prerequites_delegate = nullptr);

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory();

base::StackSamplingProfiler::UnwindersFactory
CreateLibunwindstackUnwinderFactory();

#endif  // CHROME_COMMON_PROFILER_UNWIND_UTIL_H_
