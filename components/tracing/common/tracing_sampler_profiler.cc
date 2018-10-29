// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_sampler_profiler.h"

#include <cinttypes>

#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#include <dlfcn.h>

#include "base/trace_event/cfi_backtrace_android.h"
#include "components/tracing/common/native_stack_sampler_android.h"
#endif

namespace tracing {

namespace {

std::string GetFrameNameFromOffsetAddr(uintptr_t offset_from_module_base) {
  return base::StringPrintf("off:0x%" PRIxPTR, offset_from_module_base);
}

// This class will receive the sampling profiler stackframes and output them
// to the chrome trace via an event.
class TracingProfileBuilder
    : public base::StackSamplingProfiler::ProfileBuilder {
 public:
  void OnSampleCompleted(
      std::vector<base::StackSamplingProfiler::Frame> frames) override {
    if (frames.empty()) {
      TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                           "StackCpuSampling", TRACE_EVENT_SCOPE_THREAD,
                           "frames", "empty");

      return;
    }
    // Insert an event with the frames rendered as a string with the following
    // formats:
    //   offset - module [debugid]
    //  [OR]
    //   symbol - module []
    // The offset is difference between the load module address and the
    // frame address.
    //
    // Example:
    //
    //   "malloc             - libc.so    []
    //    std::string::alloc - stdc++.so  []
    //    off:7ffb3f991b2d   - USER32.dll [2103C0950C7DEC7F7AAA44348EDC1DDD1]
    //    off:7ffb3d439164   - win32u.dll [B3E4BE89CA7FB42A2AC1E1C475284CA11]
    //    off:7ffaf3e26201   - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
    //    off:7ffaf3e26008   - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
    //    [...] "
    std::string result;
    for (const auto& frame : frames) {
      std::string frame_name;
      std::string module_name;
#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
      Dl_info info = {};
      // For chrome address we do not have symbols on the binary. So, just write
      // the offset address. For addresses on framework libraries, symbolize
      // and write the function name.
      if (base::trace_event::CFIBacktraceAndroid::is_chrome_address(
              frame.instruction_pointer)) {
        frame_name = GetFrameNameFromOffsetAddr(
            frame.instruction_pointer -
            base::trace_event::CFIBacktraceAndroid::executable_start_addr());
      } else if (dladdr(reinterpret_cast<void*>(frame.instruction_pointer),
                        &info) != 0) {
        // TODO(ssid): Add offset and module debug id if symbol was not resolved
        // in case this might be useful to send report to vendors.
        if (info.dli_sname)
          frame_name = info.dli_sname;
        if (info.dli_fname)
          module_name = info.dli_fname;
      }
      // If no module is available, then name it unknown. Adding PC would be
      // useless anyway.
      if (frame_name.empty())
        frame_name = "Unknown";
#else
      module_name = frame.module.filename.BaseName().MaybeAsASCII();
      frame_name = GetFrameNameFromOffsetAddr(frame.instruction_pointer -
                                              frame.module.base_address);
#endif
      base::StringAppendF(&result, "%s - %s [%s]\n", frame_name.c_str(),
                          module_name.c_str(), frame.module.id.c_str());
    }

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                         "StackCpuSampling", TRACE_EVENT_SCOPE_THREAD, "frames",
                         result);
  }

  void OnProfileCompleted(base::TimeDelta profile_duration,
                          base::TimeDelta sampling_period) override {}
};

}  // namespace

TracingSamplerProfiler::TracingSamplerProfiler(
    base::PlatformThreadId sampled_thread_id)
    : sampled_thread_id_(sampled_thread_id), weak_ptr_factory_(this) {
  // Make sure tracing system notices profiler category.
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"));

  DCHECK_NE(sampled_thread_id_, base::kInvalidThreadId);

  // In case tracing is currently running, start the sample profiler. The trace
  // category can be enabled only if tracing is enabled.
  OnTraceLogEnabled();
}

TracingSamplerProfiler::~TracingSamplerProfiler() {
  base::trace_event::TraceLog::GetInstance()->RemoveAsyncEnabledStateObserver(
      this);
}

void TracingSamplerProfiler::OnTraceLogEnabled() {
  // Ensure there was not an instance of the profiler already running.
  if (profiler_.get())
    return;

  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                                     &enabled);
  if (!enabled)
    return;

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = base::TimeDelta::FromMilliseconds(50);
  // If the sampled thread is stopped for too long for sampling then it is ok to
  // get next sample at a later point of time. We do not want very accurate
  // metrics when looking at traces.
  params.keep_consistent_sampling_interval = false;

  // Create and start the stack sampling profiler.
#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_id_, params, std::make_unique<TracingProfileBuilder>(),
      std::make_unique<NativeStackSamplerAndroid>(sampled_thread_id_));
#else
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_id_, params, std::make_unique<TracingProfileBuilder>());
#endif
  profiler_->Start();
}

void TracingSamplerProfiler::OnTraceLogDisabled() {
  if (!profiler_.get())
    return;
  // Stop and release the stack sampling profiler.
  profiler_->Stop();
  profiler_.reset();
}

void TracingSamplerProfiler::OnMessageLoopStarted() {
  base::trace_event::TraceLog::GetInstance()->AddAsyncEnabledStateObserver(
      weak_ptr_factory_.GetWeakPtr());
  OnTraceLogEnabled();
}

}  // namespace tracing
