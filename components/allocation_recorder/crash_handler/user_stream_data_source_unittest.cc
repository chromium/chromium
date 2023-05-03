// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/user_stream_data_source.h"

#include "base/debug/allocation_trace.h"
#include "components/allocation_recorder/testing/mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"

using allocation_recorder::testing::crash_handler::StreamDataSourceFactoryMock;
using base::debug::tracer::AllocationTraceRecorder;
using crashpad::test::TestProcessSnapshot;

namespace allocation_recorder::crash_handler {
namespace {

class AllocationRecorderStreamDataSourceTest : public ::testing::Test {
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
 protected:
  AllocationTraceRecorder& GetOperationTrace() const {
    return *operation_trace_;
  }

 private:
  const std::unique_ptr<AllocationTraceRecorder> operation_trace_ =
      std::make_unique<AllocationTraceRecorder>();
#endif
};

}  // namespace

TEST_F(AllocationRecorderStreamDataSourceTest, VerifyConstructor) {
  AllocationRecorderStreamDataSource subject_under_test(
      base::MakeRefCounted<StreamDataSourceFactoryMock>());
}

TEST_F(AllocationRecorderStreamDataSourceTest, VerifyProduceStreamData) {
  TestProcessSnapshot process_snap_shot;
  const auto stream_data_source_factory_mock =
      base::MakeRefCounted<StreamDataSourceFactoryMock>();

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  EXPECT_CALL(*stream_data_source_factory_mock, CreateReportStream()).Times(1);
#else
  EXPECT_CALL(*stream_data_source_factory_mock,
              CreateErrorMessage(Eq("!!NO ALLOCATION RECORDER AVAILABLE!!")))
      .Times(1);
#endif

  AllocationRecorderStreamDataSource subject_under_test(
      stream_data_source_factory_mock);

  subject_under_test.ProduceStreamData(&process_snap_shot);
}

}  // namespace allocation_recorder::crash_handler
