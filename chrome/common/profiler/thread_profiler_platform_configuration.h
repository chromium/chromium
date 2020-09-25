// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_
#define CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_

#include <memory>

#include "components/version_info/version_info.h"

// Encapsulates the platform-specific configuration for the ThreadProfiler.
//
// The interface functions this class make a distinction between 'supported' and
// 'enabled' state. Supported means the profiler can be run in *some*
// circumstances for *some* fraction of the population on the
// platform/branding/channel combination. This state is intended to enable
// experiment reporting. This avoids spamming UMA with experiment state on
// platforms/channels where the profiler is not being run.
//
// Enabled means we chose to the run the profiler on at least some threads on a
// platform/branding/channel combination that is configured for profiling. The
// overall enable/disable state should be reported to UMA in this case.
class ThreadProfilerPlatformConfiguration {
 public:
  virtual ~ThreadProfilerPlatformConfiguration() = default;

  // Create the platform configuration.
  static std::unique_ptr<ThreadProfilerPlatformConfiguration> Create(
      bool browser_test_mode_enabled);

  // True if the platform supports the StackSamplingProfiler and the profiler is
  // to be run for the channel/chrome branding.
  bool IsSupported(bool is_chrome_branded, version_info::Channel channel) const;

 protected:
  // True if the profiler is to be run for the channel/chrome branding on the
  // platform. Does not need to check whether the StackSamplingProfiler is
  // supported on the platform since that's done in IsSupported().
  virtual bool IsSupportedForChannel(bool is_chrome_branded,
                                     version_info::Channel channel) const = 0;
};

#endif  // CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_
