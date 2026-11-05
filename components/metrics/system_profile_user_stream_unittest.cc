// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_profile_user_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

constexpr char kUserStreamOverflowHistogram[] =
    "UMA.SystemProfile.UserStreamOverflow";

struct TestParam {
  const char* test_name;
  size_t payload_size;
  bool expect_writer_called;
  int expected_overflow_bytes;
};

class SystemProfileUserStreamTest : public testing::TestWithParam<TestParam> {};

TEST_P(SystemProfileUserStreamTest, Scenarios) {
  const TestParam& param = GetParam();
  base::HistogramTester histogram_tester;

  const std::string payload(param.payload_size, 'A');

  testing::StrictMock<
      testing::MockFunction<bool(uint32_t, base::span<const uint8_t>)>>
      writer;
  if (param.expect_writer_called) {
    EXPECT_CALL(writer,
                Call(kSystemProfileUserStreamType,
                     testing::ElementsAreArray(base::as_byte_span(payload))));
  }

  WriteSystemProfileUserStream(payload, writer.AsStdFunction());

  histogram_tester.ExpectUniqueSample(kUserStreamOverflowHistogram,
                                      param.expected_overflow_bytes,
                                      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemProfileUserStreamTest,
    testing::Values(
        TestParam{
            .test_name = "EmptyPayloadWritten",
            .payload_size = 0,
            .expect_writer_called = true,
            .expected_overflow_bytes = 0,
        },
        TestParam{
            .test_name = "TypicalPayloadWritten",
            .payload_size = 256,
            .expect_writer_called = true,
            .expected_overflow_bytes = 0,
        },
        TestParam{
            .test_name = "ExactCapacityBoundaryWritten",
            .payload_size = kSystemProfileUserStreamCapacityBytes,
            .expect_writer_called = true,
            .expected_overflow_bytes = 0,
        },
        TestParam{
            .test_name = "OneByteOverCapacityNotWritten",
            .payload_size = kSystemProfileUserStreamCapacityBytes + 1,
            .expect_writer_called = false,
            .expected_overflow_bytes = 1,
        },
        TestParam{
            .test_name = "LargeOverflowNotWritten",
            .payload_size = kSystemProfileUserStreamCapacityBytes + 7232,
            .expect_writer_called = false,
            .expected_overflow_bytes = 7232,
        }),
    [](const testing::TestParamInfo<TestParam>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace metrics
