// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/proto_string_bytes_conversion.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::ElementsAre;
using testing::Eq;

TEST(ProtoStringBytesConversionTest, ShouldAssignBytesToProtoString) {
  std::string proto_string;
  AssignBytesToProtoString(std::vector<uint8_t>{65, 66, 67}, &proto_string);
  EXPECT_THAT(proto_string, Eq("ABC"));
}

TEST(ProtoStringBytesConversionTest, ShouldConvertProtoStringToBytes) {
  const std::string proto_string = "ABC";
  EXPECT_THAT(ProtoStringToBytes(proto_string), ElementsAre(65, 66, 67));
}

}  // namespace

}  // namespace trusted_vault
