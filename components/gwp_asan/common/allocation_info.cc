// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/allocation_info.h"

#include "base/debug/stack_trace.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <pthread.h>
#endif

namespace gwp_asan::internal {

size_t AllocationInfo::GetStackTrace(base::span<const void*> trace) {
  // TODO(vtsyrklevich): Investigate using trace_event::CFIBacktraceAndroid
  // on 32-bit Android for canary/dev (where we can dynamically load unwind
  // data.)
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  // Android release builds ship without unwind tables so the normal method of
  // stack trace collection for base::debug::StackTrace doesn't work; however,
  // AArch64 builds ship with frame pointers so we can still collect stack
  // traces in that case.
  return base::debug::TraceStackFramePointers(trace, 0);
#else
  return base::debug::CollectStackTrace(trace);
#endif
}

// Report a tid that matches what crashpad collects which may differ from what
// base::PlatformThread::CurrentId() returns.
uint64_t AllocationInfo::GetCurrentTid() {
#if !BUILDFLAG(IS_APPLE)
  return base::PlatformThread::CurrentId();
#else
  uint64_t tid = base::kInvalidThreadId;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#endif
}

}  // namespace gwp_asan::internal
