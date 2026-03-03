// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/unknown_field_util.h"

#include "components/sync/test/unknown_fields.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer::test {
namespace {

TEST(UnknownFieldUtilTest, AddAndGetUnknownField) {
  sync_pb::test::UnknownFields proto;
  const std::string kValue = "unknown_value";

  AddUnknownFieldToProto(proto, kValue);

  EXPECT_EQ(GetUnknownFieldValueFromProto(proto), kValue);
}

TEST(UnknownFieldUtilTest, GetMissingUnknownField) {
  sync_pb::test::UnknownFields proto;

  EXPECT_TRUE(GetUnknownFieldValueFromProto(proto).empty());
}

TEST(UnknownFieldUtilTest, AddAndGetUnknownEnumField) {
  sync_pb::test::UnknownFields proto;
  const int kFieldNumber = 12345;
  const int kValue = 67890;

  AddUnknownEnumFieldToProto(proto, kFieldNumber, kValue);

  EXPECT_EQ(GetUnknownEnumFieldValueFromProto(proto, kFieldNumber), kValue);
}

TEST(UnknownFieldUtilTest, GetMissingUnknownEnumField) {
  sync_pb::test::UnknownFields proto;
  const int kFieldNumber = 12345;

  EXPECT_EQ(GetUnknownEnumFieldValueFromProto(proto, kFieldNumber), -1);
}

TEST(UnknownFieldUtilTest, MultipleUnknownFields) {
  sync_pb::test::UnknownFields proto;
  const int kFieldNumber1 = 12345;
  const int kValue1 = 111;
  const int kFieldNumber2 = 12346;
  const int kValue2 = 222;

  AddUnknownEnumFieldToProto(proto, kFieldNumber1, kValue1);
  AddUnknownEnumFieldToProto(proto, kFieldNumber2, kValue2);

  EXPECT_EQ(GetUnknownEnumFieldValueFromProto(proto, kFieldNumber1), kValue1);
  EXPECT_EQ(GetUnknownEnumFieldValueFromProto(proto, kFieldNumber2), kValue2);
}

TEST(UnknownFieldUtilTest, OverwriteUnknownEnumField) {
  sync_pb::test::UnknownFields proto;
  const int kFieldNumber = 12345;
  const int kValue1 = 111;
  const int kValue2 = 222;

  AddUnknownEnumFieldToProto(proto, kFieldNumber, kValue1);
  AddUnknownEnumFieldToProto(proto, kFieldNumber, kValue2);

  // Last one wins.
  EXPECT_EQ(GetUnknownEnumFieldValueFromProto(proto, kFieldNumber), kValue2);
}

TEST(UnknownFieldUtilTest, GetUnknownEnumFieldWithWrongType) {
  sync_pb::test::UnknownFields proto;
  const std::string kValue = "unknown_value";
  // This sets field 100000 which is a string.
  AddUnknownFieldToProto(proto, kValue);

  // Try to read field 100000 as an enum (varint).
  EXPECT_EQ(GetUnknownEnumFieldValueFromProto(
                proto, sync_pb::test::UnknownFields::kUnknownFieldFieldNumber),
            -1);
}

TEST(UnknownFieldUtilTest, HasUnknownFieldMatcher) {
  sync_pb::test::UnknownFields proto;
  const std::string kValue = "unknown_value";

  AddUnknownFieldToProto(proto, kValue);

  EXPECT_THAT(proto, HasUnknownField(kValue));
  EXPECT_THAT(proto, testing::Not(HasUnknownField("other_value")));
}

}  // namespace
}  // namespace syncer::test
