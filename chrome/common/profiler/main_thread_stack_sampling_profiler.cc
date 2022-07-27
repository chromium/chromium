// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler.h"
#include "chrome/common/profiler/unwind_util.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_switches.h"

namespace {

// Returns the profiler appropriate for the current process.
std::unique_ptr<ThreadProfiler> CreateThreadProfiler(
    const metrics::CallStackProfileParams::Process process) {
  // TODO(wittman): Do this for other process types too.
  if (process == metrics::CallStackProfileParams::Process::kBrowser) {
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::BindRepeating(
            &metrics::CallStackProfileMetricsProvider::ReceiveProfile));
    return ThreadProfiler::CreateAndStartOnMainThread();
  }

  // No other processes are currently supported.
  return nullptr;
}

}  // namespace

MainThreadStackSamplingProfiler::MainThreadStackSamplingProfiler() {
  const metrics::CallStackProfileParams::Process process =
      GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess());

// We only want to incur the cost of universally downloading the module in
// early channels, where profiling will occur over substantially all of
// the population. When supporting later channels in the future we will
// enable profiling for only a fraction of users and only download for
// those users.
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (process == metrics::CallStackProfileParams::Process::kBrowser &&
      !UnwindPrerequisites::Available()) {
    const version_info::Channel channel = chrome::GetChannel();
    if (channel == version_info::Channel::CANARY ||
        channel == version_info::Channel::DEV) {
      UnwindPrerequisites::RequestInstallation();
    }
  }
#endif

  sampling_profiler_ = CreateThreadProfiler(process);
}

// Note that it's important for the |sampling_profiler_| destructor to run, as
// it ensures program correctness on shutdown. Without it, the profiler thread's
// destruction can race with the profiled thread's destruction, which results in
// the sampling thread attempting to profile the sampled thread after the
// sampled thread has already been shut down.
MainThreadStackSamplingProfiler::~MainThreadStackSamplingProfiler() = default;
