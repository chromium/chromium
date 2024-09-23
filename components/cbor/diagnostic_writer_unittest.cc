// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/diagnostic_writer.h"

#include "base/strings/stringprintf.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cbor {

TEST(CBORDiagnosticWriterTest, Basic) {
  Value::MapValue map;
  map.emplace(1, 1);
  map.emplace(2, -2);
  map.emplace(3, "test");
  std::vector<uint8_t> bytes = {1, 2, 3, 4};
  map.emplace(4, std::move(bytes));

  Value::MapValue submap;
  submap.emplace(5, true);
  submap.emplace(6, false);
  map.emplace(5, cbor::Value(submap));

  Value::ArrayValue array;
  array.emplace_back(1);
  array.emplace_back(2);
  array.emplace_back(3);
  array.emplace_back("foo");
  map.emplace(6, cbor::Value(array));

  map.emplace(7, "es\'cap\\in\ng");

  map.emplace(8, cbor::Value(3.14));

  EXPECT_EQ(
      "{1: 1, 2: -2, 3: \"test\", 4: h'01020304', 5: {5: true, 6: false}, 6: "
      "[1, 2, 3, \"foo\"], 7: \"es'cap\\\\in\\ng\", 8: 3.14}",
      DiagnosticWriter::Write(cbor::Value(map)));
}

TEST(CBORDiagnosticWriterTest, SizeLimit) {
  Value::ArrayValue array;
  array.emplace_back(1);
  array.emplace_back(2);
  array.emplace_back(3);
  EXPECT_EQ("[1, 2, 3]", DiagnosticWriter::Write(cbor::Value(array)));
  // A limit of zero is set, but it's only rough, so a few bytes might be
  // produced.
  EXPECT_LT(
      DiagnosticWriter::Write(cbor::Value(array), /*rough_max_output_bytes=*/0)
          .size(),
      3u);

  std::vector<uint8_t> bytes;
  bytes.resize(100);
  EXPECT_LT(
      DiagnosticWriter::Write(cbor::Value(bytes), /*rough_max_output_bytes=*/0)
          .size(),
      20u);
}

TEST(CBORDiagnosticWriterTest, LargeBytestrings) {
  constexpr struct {
    size_t length;
    bool should_be_truncated;
  } kTestCases[] = {
      {0, false},
      {1, false},
      // Just under the 87.5% threshold.
      {56, false},
      // Just over the 87.5% threshold.
      {57, true},
      // 100% of the output limit.
      {64, true},
      // Over the output limit.
      {65, true},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.length);

    Value::ArrayValue array;
    array.emplace_back(1);
    array.emplace_back(std::vector<uint8_t>(test.length, 0));
    array.emplace_back(3);

    std::string expected;
    if (test.should_be_truncated) {
      expected = base::StringPrintf("[1, (%zu bytes), 3]", test.length);
    } else {
      expected = base::StringPrintf("[1, h'%s', 3]",
                                    std::string(test.length * 2, '0').c_str());
    }

    EXPECT_EQ(expected,
              DiagnosticWriter::Write(cbor::Value(array),
                                      /*rough_max_output_bytes=*/128));
  }
}

TEST(CBORDiagnosticWriterTest, InvalidUTF8) {
  static const uint8_t kInvalidUTF8[] = {0x62, 0xe2, 0x80};
  cbor::Reader::Config config;
  config.allow_invalid_utf8 = true;
  std::optional<cbor::Value> maybe_value =
      cbor::Reader::Read(kInvalidUTF8, config);

  ASSERT_TRUE(maybe_value);
  EXPECT_EQ("s'E280'", DiagnosticWriter::Write(*maybe_value));
}

}  // namespace cbor
