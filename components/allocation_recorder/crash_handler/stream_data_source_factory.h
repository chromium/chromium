// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_STREAM_DATA_SOURCE_FACTORY_H_
#define COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_STREAM_DATA_SOURCE_FACTORY_H_

#include <memory>
#include <string_view>

#include "base/debug/debugging_buildflags.h"
#include "base/memory/ref_counted.h"

namespace base::debug::tracer {
class AllocationTraceRecorder;
}

namespace crashpad {
class MinidumpUserExtensionStreamDataSource;
}

namespace allocation_recorder::crash_handler {

// A factory to create the various message that may be written to the crashpad
// stream.
class StreamDataSourceFactory
    : public base::RefCounted<StreamDataSourceFactory> {
 public:
  virtual std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  CreateErrorMessage(std::string_view error_message) const;

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  virtual std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  CreateReportStream(
      const base::debug::tracer::AllocationTraceRecorder& recorder) const;
#endif

 protected:
  virtual ~StreamDataSourceFactory();

 private:
  friend class base::RefCounted<StreamDataSourceFactory>;
};

}  // namespace allocation_recorder::crash_handler

#endif  // COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_STREAM_DATA_SOURCE_FACTORY_H_
