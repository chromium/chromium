// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_client/client.h"

#include "base/check_op.h"
#include "base/debug/allocation_trace.h"
#include "build/build_config.h"
#include "components/allocation_recorder/internal/internal.h"
#include "third_party/crashpad/crashpad/client/annotation.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/core/app/crashpad.h"  // nogncheck
#endif

using allocation_recorder::internal::kAnnotationName;
using allocation_recorder::internal::kAnnotationType;
using base::debug::tracer::AllocationTraceRecorder;

namespace allocation_recorder::crash_client {

namespace {

// Annotations of Crashpad can have only up to
// crashpad::Annotation::kValueMaxSize bytes. AllocationTraceRecorder usually
// has a much larger size. Due to this fact we use a two staged approach. We
// pass the exact address of the Recorder in an annotation and allow Crashpad
// to access this specific address range.
//
// The crashpad annotation ensures that a sequence of N bytes is copied from a
// given address into the crash handler. So, in order to make the address of
// the recorder accessible from the crash handler, we have to store it
// explicitly in a data field that is still alive at crash time. If we used
// the recorder's address directly, we'd end up copying some bytes of the
// recorder only, which doesn't work due to size limitations (see the
// static_assert below).

// Ensure we take this double indirection approach only when necessary. That
// is the size of AllocationTraceRecorder must exceed the maximum amount of
// data that an annotation can hold.
static_assert(sizeof(AllocationTraceRecorder) >
              crashpad::Annotation::kValueMaxSize);

uintptr_t g_recorder_address = 0;

crashpad::Annotation g_recorder_address_annotation(
    kAnnotationType,
    kAnnotationName,
    static_cast<void*>(&g_recorder_address));

}  // namespace

void RegisterRecorderWithCrashpad(AllocationTraceRecorder& trace_recorder) {
  CHECK_EQ(g_recorder_address, 0ul);

  g_recorder_address = reinterpret_cast<uintptr_t>(&trace_recorder);
  g_recorder_address_annotation.SetSize(sizeof(g_recorder_address));

#if BUILDFLAG(IS_ANDROID)
  crash_reporter::AllowMemoryRange(&trace_recorder, sizeof(trace_recorder));
#endif
}

void UnregisterRecorderWithCrashpad() {
  CHECK_NE(g_recorder_address, 0ul);

  g_recorder_address = 0;
  g_recorder_address_annotation.Clear();

  // We do not remove the recorder from the allowed memory ranges on Android
  // since the crash reporter doesn't offer the functionality to do so. This
  // shouldn't be a problem since we disabled the annotation effectively when
  // clearing it.
}

}  // namespace allocation_recorder::crash_client
