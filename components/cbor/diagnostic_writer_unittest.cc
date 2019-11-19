// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/diagnostic_writer.h"

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

  EXPECT_EQ(
      "{1: 1, 2: -2, 3: \"test\", 4: h'01020304', 5: {5: true, 6: false}, 6: "
      "[1, 2, 3, \"foo\"], 7: \"es'cap\\\\in\\ng\"}",
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
      3u);
}

TEST(CBORDiagnosticWriterTest, InvalidUTF8) {
  static const uint8_t kInvalidUTF8[] = {0x62, 0xe2, 0x80};
  cbor::Reader::Config config;
  config.allow_invalid_utf8 = true;
  base::Optional<cbor::Value> maybe_value =
      cbor::Reader::Read(kInvalidUTF8, config);

  ASSERT_TRUE(maybe_value);
  EXPECT_EQ("s'E280'", DiagnosticWriter::Write(*maybe_value));
}

}  // namespace cbor
