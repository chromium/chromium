// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/stream_data_source_factory.h"

#include <vector>

#include "base/debug/allocation_trace.h"
#include "components/allocation_recorder/internal/internal.h"
#include "components/allocation_recorder/testing/crashpad_fake_objects.h"
#include "components/allocation_recorder/testing/mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::debug::tracer::AllocationTraceRecorder;
using crashpad::test::BufferExtensionStreamDataSourceDelegate;

namespace allocation_recorder::crash_handler {
namespace {
class StreamDataSourceFactoryTest : public ::testing::Test {
 protected:
  StreamDataSourceFactory& GetSubjectUnderTest() { return *sut_; }

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  AllocationTraceRecorder& GetOperationTraceRecorder() const {
    return *operation_trace_;
  }
#endif

 private:
  const scoped_refptr<StreamDataSourceFactory> sut_ =
      base::MakeRefCounted<StreamDataSourceFactory>();

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  const std::unique_ptr<AllocationTraceRecorder> operation_trace_ =
      std::make_unique<AllocationTraceRecorder>();
#endif
};

// Read the message stored in the given stream and return it.
std::vector<uint8_t> ReadMessage(
    const std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>&
        stream) {
  BufferExtensionStreamDataSourceDelegate stream_data_source_delegate;

  stream->ReadStreamData(&stream_data_source_delegate);

  return stream_data_source_delegate.GetMessage();
}

// Verify the header and footer of the message match internal::kReportMarker.
// The payload_data will contain begin and end of the payload for further
// verification.
void VerifyMessageHeaderAndFooter(
    const std::vector<uint8_t>& received_message,
    std::pair<std::vector<uint8_t>::const_iterator,
              std::vector<uint8_t>::const_iterator>& payload_data) {
  // Assert here as we might continue reading out of bounds with an EXPECT_EQ.
  ASSERT_GE(received_message.size(), 2 * internal::kLengthOfReportMarker);

  const auto* const expected_header_begin =
      std::cbegin(internal::kReportMarker);
  // Do not use std::end to calculate the end of the expected header as this
  // would include the terminating 0.
  const auto* const expected_header_end =
      std::cbegin(internal::kReportMarker) + internal::kLengthOfReportMarker;

  // Check the header.
  const auto intro_compare = std::mismatch(
      expected_header_begin, expected_header_end, received_message.begin());
  EXPECT_EQ(intro_compare.first, expected_header_end)
      << "difference at intro-position="
      << std::distance(expected_header_begin, intro_compare.first)
      << ", expected='" << *intro_compare.first << "', found='"
      << *intro_compare.second << "'.";

  const auto received_payload_begin =
      std::cbegin(received_message) + internal::kLengthOfReportMarker;
  const auto received_payload_end =
      std::cend(received_message) - internal::kLengthOfReportMarker;

  // Check the footer.
  const auto outro_compare = std::mismatch(
      expected_header_begin, expected_header_end, received_payload_end);
  EXPECT_EQ(outro_compare.first, expected_header_end)
      << "difference at outro-position="
      << std::distance(expected_header_begin, outro_compare.first)
      << ", expected='" << *outro_compare.first << "', found='"
      << *outro_compare.second << "'.";

  payload_data = {received_payload_begin, received_payload_end};
}

void VerifyMessageFromStream(
    const std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>&
        stream,
    const std::string& expected_payload_param) {
  ASSERT_NE(stream, nullptr);

  std::vector<uint8_t> expected_payload;
  std::transform(
      std::begin(expected_payload_param), std::end(expected_payload_param),
      std::back_inserter(expected_payload),
      [](std::string::value_type c) { return static_cast<uint8_t>(c); });

  const std::vector<uint8_t> received_message = ReadMessage(stream);

  std::pair<std::vector<uint8_t>::const_iterator,
            std::vector<uint8_t>::const_iterator>
      payload_range;

  VerifyMessageHeaderAndFooter(received_message, payload_range);

  ASSERT_EQ(std::ssize(expected_payload),
            std::distance(payload_range.first, payload_range.second));

  const auto payload_begin = payload_range.first;
  const auto expected_payload_begin = expected_payload.begin();
  const auto expected_payload_end = expected_payload.end();
  const auto message_compare = std::mismatch(
      expected_payload_begin, expected_payload_end, payload_begin);

  EXPECT_EQ(message_compare.first, expected_payload_end)
      << "difference at payload-position="
      << std::distance(expected_payload_begin, message_compare.first)
      << ", expected='" << *message_compare.first << "', found='"
      << *message_compare.second << "'.";
}
}  // namespace

TEST_F(StreamDataSourceFactoryTest, VerifyCreateErrorMessage) {
  StreamDataSourceFactory& sut = GetSubjectUnderTest();

  const char* const error = "A SUPER FATAL ERROR HAPPENED";

  VerifyMessageFromStream(sut.CreateErrorMessage(error), std::string{error});
}

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
TEST_F(StreamDataSourceFactoryTest, VerifyCreateReportStream) {
  StreamDataSourceFactory& sut = GetSubjectUnderTest();

  VerifyMessageFromStream(sut.CreateReportStream(),
                          std::string{"!!REPORT CREATION NOT IMPLEMENTED!!"});
}
#endif

}  // namespace allocation_recorder::crash_handler
