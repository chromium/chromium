// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_ALLOCATION_RECORDER_HOLDER_H_
#define COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_ALLOCATION_RECORDER_HOLDER_H_

#include <memory>
#include <string>

#include "base/debug/allocation_trace.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"

namespace crashpad {
class ProcessSnapshot;
}

namespace allocation_recorder::crash_handler {

// The result of the initialization of a AllocationRecorderHolder.
using Result =
    base::expected<const base::debug::tracer::AllocationTraceRecorder*,
                   std::string>;

// AllocationRecorderHolder is responsible for loading an
// AllocationTraceRecorder from client memory as passed by process snapshot. It
// provides a buffer to allow reading data from the recorder in the crash
// handler.
class AllocationRecorderHolder
    : public base::RefCounted<AllocationRecorderHolder> {
 public:
  // Load an AllocationStackTraceRecorder from the passed process snapshot and
  // provide access via the result.
  virtual Result Initialize(const crashpad::ProcessSnapshot& process_snapshot);

 protected:
  virtual ~AllocationRecorderHolder();

 private:
  friend class base::RefCounted<AllocationRecorderHolder>;

  // The memory area used to store the recorder for further processing here in
  // the crash handler.
  alignas(base::debug::tracer::AllocationTraceRecorder)
      uint8_t buffer_[sizeof(base::debug::tracer::AllocationTraceRecorder)];
};

}  // namespace allocation_recorder::crash_handler

#endif  // COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_ALLOCATION_RECORDER_HOLDER_H_
