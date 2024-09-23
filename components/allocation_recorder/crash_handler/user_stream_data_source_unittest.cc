// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/user_stream_data_source.h"

#include <memory>

#include "base/debug/allocation_trace.h"
#include "base/memory/scoped_refptr.h"
#include "components/allocation_recorder/testing/mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"

using allocation_recorder::testing::crash_handler::AllocationRecorderHolderMock;
using allocation_recorder::testing::crash_handler::StreamDataSourceFactoryMock;
using base::debug::tracer::AllocationTraceRecorder;
using crashpad::test::TestProcessSnapshot;
using ::testing::_;
using ::testing::Eq;
using ::testing::Ref;
using ::testing::Return;

namespace allocation_recorder::crash_handler {
namespace {

class AllocationRecorderStreamDataSourceTest : public ::testing::Test {
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
 protected:
  AllocationTraceRecorder& GetAllocationRecorder() const {
    return *allocation_recorder_;
  }

 private:
  const std::unique_ptr<AllocationTraceRecorder> allocation_recorder_ =
      std::make_unique<AllocationTraceRecorder>();
#endif
};

}  // namespace

TEST_F(AllocationRecorderStreamDataSourceTest, VerifyConstructor) {
  AllocationRecorderStreamDataSource subject_under_test(
      base::MakeRefCounted<AllocationRecorderHolderMock>(),
      base::MakeRefCounted<StreamDataSourceFactoryMock>());
}

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
TEST_F(AllocationRecorderStreamDataSourceTest, VerifyProduceStreamData) {
  TestProcessSnapshot process_snap_shot;
  AllocationTraceRecorder& allocation_recorder = GetAllocationRecorder();

  const auto allocation_trace_recorder_holder_mock =
      base::MakeRefCounted<AllocationRecorderHolderMock>();
  const auto stream_data_source_factory_mock =
      base::MakeRefCounted<StreamDataSourceFactoryMock>();

  EXPECT_CALL(*allocation_trace_recorder_holder_mock,
              Initialize(Ref(process_snap_shot)))
      .WillOnce(Return(base::ok(&allocation_recorder)));
  EXPECT_CALL(*stream_data_source_factory_mock,
              CreateReportStream(Ref(allocation_recorder)))
      .Times(1);

  AllocationRecorderStreamDataSource subject_under_test(
      allocation_trace_recorder_holder_mock, stream_data_source_factory_mock);

  subject_under_test.ProduceStreamData(&process_snap_shot);
}

TEST_F(AllocationRecorderStreamDataSourceTest,
       VerifyErrorWhenInitializationOfHolderFails) {
  TestProcessSnapshot process_snap_shot;
  auto const allocation_trace_recorder_holder_mock =
      base::MakeRefCounted<AllocationRecorderHolderMock>();
  auto const stream_data_source_factory_mock =
      base::MakeRefCounted<StreamDataSourceFactoryMock>();

  EXPECT_CALL(*allocation_trace_recorder_holder_mock,
              Initialize(Ref(process_snap_shot)))
      .WillOnce(Return(base::unexpected("some test error")));
  EXPECT_CALL(*stream_data_source_factory_mock, CreateReportStream(_)).Times(0);
  EXPECT_CALL(*stream_data_source_factory_mock,
              CreateErrorMessage(Eq("some test error")))
      .Times(1);

  AllocationRecorderStreamDataSource subject_under_test(
      allocation_trace_recorder_holder_mock, stream_data_source_factory_mock);

  subject_under_test.ProduceStreamData(&process_snap_shot);
}

#else

TEST_F(AllocationRecorderStreamDataSourceTest,
       VerifyProduceStreamDataNullAllocationTraceRecorder) {
  TestProcessSnapshot process_snap_shot_mock;
  auto const allocation_trace_recorder_holder_mock =
      base::MakeRefCounted<AllocationRecorderHolderMock>();
  auto const stream_data_source_factory_mock =
      base::MakeRefCounted<StreamDataSourceFactoryMock>();

  EXPECT_CALL(*allocation_trace_recorder_holder_mock, Initialize(_)).Times(0);
  EXPECT_CALL(*stream_data_source_factory_mock,
              CreateErrorMessage(Eq("!!NO ALLOCATION RECORDER AVAILABLE!!")))
      .Times(1);

  AllocationRecorderStreamDataSource subject_under_test(
      allocation_trace_recorder_holder_mock, stream_data_source_factory_mock);

  subject_under_test.ProduceStreamData(&process_snap_shot_mock);
}

#endif

}  // namespace allocation_recorder::crash_handler
