// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_TESTING_MOCK_OBJECTS_H_
#define COMPONENTS_ALLOCATION_RECORDER_TESTING_MOCK_OBJECTS_H_

#include <memory>
#include <string_view>

#include "components/allocation_recorder/crash_handler/allocation_recorder_holder.h"
#include "components/allocation_recorder/crash_handler/stream_data_source_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace allocation_recorder::testing::crash_handler {

struct StreamDataSourceFactoryMock
    : public allocation_recorder::crash_handler::StreamDataSourceFactory {
  StreamDataSourceFactoryMock();
  ~StreamDataSourceFactoryMock() override;

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  MOCK_METHOD(
      std::unique_ptr<::crashpad::MinidumpUserExtensionStreamDataSource>,
      CreateReportStream,
      (const base::debug::tracer::AllocationTraceRecorder&),
      (const override));
#endif

  MOCK_METHOD(
      std::unique_ptr<::crashpad::MinidumpUserExtensionStreamDataSource>,
      CreateErrorMessage,
      (std::string_view error_message),
      (const override));
};

struct AllocationRecorderHolderMock
    : public allocation_recorder::crash_handler::AllocationRecorderHolder {
  AllocationRecorderHolderMock();
  ~AllocationRecorderHolderMock() override;

  MOCK_METHOD(allocation_recorder::crash_handler::Result,
              Initialize,
              (const ::crashpad::ProcessSnapshot& process_snapshot),
              (override));
};

}  // namespace allocation_recorder::testing::crash_handler

#endif  // COMPONENTS_ALLOCATION_RECORDER_TESTING_MOCK_OBJECTS_H_
