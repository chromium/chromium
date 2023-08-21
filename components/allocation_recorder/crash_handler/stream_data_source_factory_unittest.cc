// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/stream_data_source_factory.h"

#include <vector>

#include "base/debug/allocation_trace.h"
#include "components/allocation_recorder/crash_handler/memory_operation_report.pb.h"
#include "components/allocation_recorder/internal/internal.h"
#include "components/allocation_recorder/testing/crashpad_fake_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::debug::tracer::AllocationTraceRecorder;
using crashpad::test::BufferExtensionStreamDataSourceDelegate;

namespace allocation_recorder::crash_handler {
namespace {

class StreamDataSourceFactoryTest : public ::testing::Test {
 protected:
  StreamDataSourceFactory& GetFactory() { return *factory_; }

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  AllocationTraceRecorder& GetRecorder() const { return *operation_trace_; }
#endif

 private:
  const scoped_refptr<StreamDataSourceFactory> factory_ =
      base::MakeRefCounted<StreamDataSourceFactory>();

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  const std::unique_ptr<AllocationTraceRecorder> operation_trace_ =
      std::make_unique<AllocationTraceRecorder>();
#endif
};

// Read the message stored in the given stream and return it.
std::string ReadMessageAsString(
    const std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>&
        stream) {
  BufferExtensionStreamDataSourceDelegate stream_data_source_delegate;

  stream->ReadStreamData(&stream_data_source_delegate);

  const std::vector<uint8_t> message = stream_data_source_delegate.GetMessage();

  return std::string(reinterpret_cast<const char*>(message.data()),
                     message.size());
}

void GetPayloadFromStream(
    const std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>&
        stream,
    allocation_recorder::Payload& payload) {
  ASSERT_NE(stream, nullptr);

  const auto received_message = ReadMessageAsString(stream);

  ASSERT_TRUE(payload.ParseFromString(received_message));
}

}  // namespace

TEST_F(StreamDataSourceFactoryTest, VerifyCreateErrorMessage) {
  StreamDataSourceFactory& factory = GetFactory();

  const char* const error = "A fatal error happened.";

  allocation_recorder::Payload payload;

  GetPayloadFromStream(factory.CreateErrorMessage(error), payload);

  ASSERT_TRUE(payload.has_processing_failures());
  EXPECT_EQ(1, payload.processing_failures().messages().size());
  EXPECT_EQ(error, payload.processing_failures().messages().at(0));
}

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
TEST_F(StreamDataSourceFactoryTest, VerifyCreateReportStream) {
  AllocationTraceRecorder& recorder = GetRecorder();
  StreamDataSourceFactory& factory = GetFactory();

  allocation_recorder::Payload payload;

  GetPayloadFromStream(factory.CreateReportStream(recorder), payload);

  EXPECT_TRUE(payload.has_operation_report());
  EXPECT_TRUE(payload.operation_report().has_statistics());
  EXPECT_EQ(0, payload.operation_report().memory_operations().size());
}
#endif

}  // namespace allocation_recorder::crash_handler
