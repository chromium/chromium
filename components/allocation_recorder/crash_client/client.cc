// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_client/client.h"

#include "base/debug/allocation_trace.h"
#include "build/build_config.h"
#include "components/allocation_recorder/internal/internal.h"
#include "third_party/crashpad/crashpad/client/annotation.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/core/app/crashpad.h"
#endif

using allocation_recorder::internal::kAnnotationName;
using allocation_recorder::internal::kAnnotationType;
using base::debug::tracer::AllocationTraceRecorder;

namespace allocation_recorder::crash_client {

AllocationTraceRecorder& Initialize() {
  // Annotations of Crashpad can have only up to
  // crashpad::Annotation::kValueMaxSize bytes. AllocationTraceRecorder usually
  // has a much larger size. Due to this fact we use a two staged approach. We
  // store the exact address of the Recorder in an annotation and allow Crashpad
  // to access this specific address range.

  static AllocationTraceRecorder trace_recorder_instance;

  // The crashpad annotation ensures that a sequence of N bytes is copied from a
  // given address into the crash handler. So, in order to make the address of
  // the recorder accessible from the crash handler, we have to store it
  // explicitly in a data field that is still alive at crash time. If we used
  // the recorder's address directly, we'd end up copying some bytes of the
  // recorder only, which doesn't work due to size limitations (see the
  // static_assert below).
  static AllocationTraceRecorder* recorder_address = &trace_recorder_instance;

  // Ensure we take this double indirection approach only when necessary. That
  // is the size of AllocationTraceRecorder must exceed the maximum amount of
  // data that an annotation can hold.
  static_assert(sizeof(trace_recorder_instance) >
                crashpad::Annotation::kValueMaxSize);

  static crashpad::Annotation allocation_stack_recorder_address(
      kAnnotationType, kAnnotationName, static_cast<void*>(&recorder_address));

  allocation_stack_recorder_address.SetSize(sizeof(recorder_address));

#if BUILDFLAG(IS_ANDROID)
  crash_reporter::AllowMemoryRange(static_cast<void*>(&trace_recorder_instance),
                                   sizeof(trace_recorder_instance));
#endif

  return trace_recorder_instance;
}

}  // namespace allocation_recorder::crash_client
