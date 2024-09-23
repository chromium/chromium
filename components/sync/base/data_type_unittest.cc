// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type.h"

#include <set>
#include <string>

#include "base/strings/string_util.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(DataTypeTest, IsRealDataType) {
  EXPECT_FALSE(IsRealDataType(UNSPECIFIED));
  EXPECT_TRUE(IsRealDataType(FIRST_REAL_DATA_TYPE));
  EXPECT_TRUE(IsRealDataType(LAST_REAL_DATA_TYPE));
  EXPECT_TRUE(IsRealDataType(BOOKMARKS));
  EXPECT_TRUE(IsRealDataType(APPS));
  EXPECT_TRUE(IsRealDataType(ARC_PACKAGE));
  EXPECT_TRUE(IsRealDataType(PRINTERS));
  EXPECT_TRUE(IsRealDataType(PRINTERS_AUTHORIZATION_SERVERS));
  EXPECT_TRUE(IsRealDataType(READING_LIST));
}

// Make sure we can convert DataTypes to and from specifics field
// numbers.
TEST(DataTypeTest, DataTypeToFromSpecificsFieldNumber) {
  DataTypeSet protocol_types = ProtocolTypes();
  for (DataType type : protocol_types) {
    int field_number = GetSpecificsFieldNumberFromDataType(type);
    EXPECT_EQ(type, GetDataTypeFromSpecificsFieldNumber(field_number));
  }
}

TEST(DataTypeTest, DataTypeOfInvalidSpecificsFieldNumber) {
  EXPECT_EQ(UNSPECIFIED, GetDataTypeFromSpecificsFieldNumber(0));
}

TEST(DataTypeTest, DataTypeHistogramMapping) {
  std::set<DataTypeForHistograms> histogram_values;
  DataTypeSet all_types = DataTypeSet::All();
  for (DataType type : all_types) {
    SCOPED_TRACE(DataTypeToDebugString(type));
    DataTypeForHistograms histogram_value = DataTypeHistogramValue(type);

    EXPECT_TRUE(histogram_values.insert(histogram_value).second)
        << "Expected histogram values to be unique";

    EXPECT_LE(static_cast<int>(histogram_value),
              static_cast<int>(DataTypeForHistograms::kMaxValue));
  }
}

TEST(DataTypeTest, DataTypeToStableIdentifier) {
  std::set<int> identifiers;
  DataTypeSet all_types = DataTypeSet::All();
  for (DataType type : all_types) {
    SCOPED_TRACE(DataTypeToDebugString(type));
    int stable_identifier = DataTypeToStableIdentifier(type);
    EXPECT_GT(stable_identifier, 0);
    EXPECT_TRUE(identifiers.insert(stable_identifier).second)
        << "Expected identifier values to be unique";
  }

  // Hard code a few example data_types to make it harder to break that the
  // identifiers are stable.
  EXPECT_EQ(3, DataTypeToStableIdentifier(BOOKMARKS));
  EXPECT_EQ(7, DataTypeToStableIdentifier(AUTOFILL));
  EXPECT_EQ(52, DataTypeToStableIdentifier(HISTORY));
}

TEST(DataTypeTest, DefaultFieldValues) {
  DataTypeSet types = ProtocolTypes();
  for (DataType type : types) {
    SCOPED_TRACE(DataTypeToDebugString(type));

    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(type, &specifics);
    EXPECT_TRUE(specifics.IsInitialized());

    std::string tmp;
    EXPECT_TRUE(specifics.SerializeToString(&tmp));

    sync_pb::EntitySpecifics from_string;
    EXPECT_TRUE(from_string.ParseFromString(tmp));
    EXPECT_TRUE(from_string.IsInitialized());

    EXPECT_EQ(type, GetDataTypeFromSpecifics(from_string));
  }
}

TEST(DataTypeTest, DataTypeToProtocolRootTagValues) {
  for (DataType data_type : ProtocolTypes()) {
    std::string root_tag = DataTypeToProtocolRootTag(data_type);
    if (IsRealDataType(data_type)) {
      EXPECT_TRUE(base::StartsWith(root_tag, "google_chrome_",
                                   base::CompareCase::INSENSITIVE_ASCII));
    } else {
      EXPECT_EQ(root_tag, "INVALID");
    }
  }
}

TEST(DataTypeTest, DataTypeDebugStringIsNotEmpty) {
  for (DataType data_type : DataTypeSet::All()) {
    EXPECT_NE("", DataTypeToDebugString(data_type));
  }
}

TEST(DataTypeTest, DataTypesSubsetsSanity) {
  // UserTypes and ControlTypes shouldn't overlap.
  EXPECT_TRUE(Intersection(UserTypes(), ControlTypes()).empty());

  // UserTypes should contain all *UserTypes.
  EXPECT_TRUE(UserTypes().HasAll(AlwaysPreferredUserTypes()));
  EXPECT_TRUE(UserTypes().HasAll(AlwaysEncryptedUserTypes()));
  EXPECT_TRUE(UserTypes().HasAll(LowPriorityUserTypes()));
  EXPECT_TRUE(UserTypes().HasAll(HighPriorityUserTypes()));

  // Low-prio types and high-prio types shouldn't overlap.
  EXPECT_TRUE(
      Intersection(LowPriorityUserTypes(), HighPriorityUserTypes()).empty());

  // The always-encrypted types should be encryptable.
  EXPECT_TRUE(EncryptableUserTypes().HasAll(AlwaysEncryptedUserTypes()));

  // Commit-only types are meant for consumption on the server, and so should
  // not be encryptable (with a custom passphrase).
  EXPECT_TRUE(Intersection(CommitOnlyTypes(), EncryptableUserTypes()).empty());
}

TEST(DataTypeTest, DataTypeSetFromSpecificsFieldNumberList) {
  // Get field numbers corresponding to each data type in ProtocolTypes().
  ::google::protobuf::RepeatedField<int> field_numbers;
  for (auto data_type : ProtocolTypes()) {
    field_numbers.Add(GetSpecificsFieldNumberFromDataType(data_type));
  }
  EXPECT_EQ(GetDataTypeSetFromSpecificsFieldNumberList(field_numbers),
            ProtocolTypes());
}

}  // namespace
}  // namespace syncer
