// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/strings/string_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(ModelTypeTest, IsRealDataType) {
  EXPECT_FALSE(IsRealDataType(UNSPECIFIED));
  EXPECT_TRUE(IsRealDataType(FIRST_REAL_MODEL_TYPE));
  EXPECT_TRUE(IsRealDataType(LAST_REAL_MODEL_TYPE));
  EXPECT_TRUE(IsRealDataType(BOOKMARKS));
  EXPECT_TRUE(IsRealDataType(APPS));
  EXPECT_TRUE(IsRealDataType(ARC_PACKAGE));
  EXPECT_TRUE(IsRealDataType(PRINTERS));
  EXPECT_TRUE(IsRealDataType(PRINTERS_AUTHORIZATION_SERVERS));
  EXPECT_TRUE(IsRealDataType(READING_LIST));
}

// Make sure we can convert ModelTypes to and from specifics field
// numbers.
TEST(ModelTypeTest, ModelTypeToFromSpecificsFieldNumber) {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    int field_number = GetSpecificsFieldNumberFromModelType(type);
    EXPECT_EQ(type, GetModelTypeFromSpecificsFieldNumber(field_number));
  }
}

TEST(ModelTypeTest, ModelTypeOfInvalidSpecificsFieldNumber) {
  EXPECT_EQ(UNSPECIFIED, GetModelTypeFromSpecificsFieldNumber(0));
}

TEST(ModelTypeTest, ModelTypeHistogramMapping) {
  std::set<ModelTypeForHistograms> histogram_values;
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType type : all_types) {
    SCOPED_TRACE(ModelTypeToDebugString(type));
    ModelTypeForHistograms histogram_value = ModelTypeHistogramValue(type);

    EXPECT_TRUE(histogram_values.insert(histogram_value).second)
        << "Expected histogram values to be unique";

    EXPECT_LE(static_cast<int>(histogram_value),
              static_cast<int>(ModelTypeForHistograms::kMaxValue));
  }
}

TEST(ModelTypeTest, ModelTypeToStableIdentifier) {
  std::set<int> identifiers;
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType type : all_types) {
    SCOPED_TRACE(ModelTypeToDebugString(type));
    int stable_identifier = ModelTypeToStableIdentifier(type);
    EXPECT_GT(stable_identifier, 0);
    EXPECT_TRUE(identifiers.insert(stable_identifier).second)
        << "Expected identifier values to be unique";
  }

  // Hard code a few example model_types to make it harder to break that the
  // identifiers are stable.
  EXPECT_EQ(3, ModelTypeToStableIdentifier(BOOKMARKS));
  EXPECT_EQ(7, ModelTypeToStableIdentifier(AUTOFILL));
  EXPECT_EQ(52, ModelTypeToStableIdentifier(HISTORY));
}

TEST(ModelTypeTest, DefaultFieldValues) {
  ModelTypeSet types = ProtocolTypes();
  for (ModelType type : types) {
    SCOPED_TRACE(ModelTypeToDebugString(type));

    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(type, &specifics);
    EXPECT_TRUE(specifics.IsInitialized());

    std::string tmp;
    EXPECT_TRUE(specifics.SerializeToString(&tmp));

    sync_pb::EntitySpecifics from_string;
    EXPECT_TRUE(from_string.ParseFromString(tmp));
    EXPECT_TRUE(from_string.IsInitialized());

    EXPECT_EQ(type, GetModelTypeFromSpecifics(from_string));
  }
}

TEST(ModelTypeTest, ModelTypeToProtocolRootTagValues) {
  for (ModelType model_type : ProtocolTypes()) {
    std::string root_tag = ModelTypeToProtocolRootTag(model_type);
    if (IsRealDataType(model_type)) {
      EXPECT_TRUE(base::StartsWith(root_tag, "google_chrome_",
                                   base::CompareCase::INSENSITIVE_ASCII));
    } else {
      EXPECT_EQ(root_tag, "INVALID");
    }
  }
}

TEST(ModelTypeTest, ModelTypeDebugStringIsNotEmpty) {
  for (ModelType model_type : ModelTypeSet::All()) {
    EXPECT_NE("", ModelTypeToDebugString(model_type));
  }
}

TEST(ModelTypeTest, ModelTypesSubsetsSanity) {
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

TEST(ModelTypeTest, ModelTypeSetFromSpecificsFieldNumberList) {
  // Get field numbers corresponding to each model type in ProtocolTypes().
  ::google::protobuf::RepeatedField<int> field_numbers;
  for (auto model_type : ProtocolTypes()) {
    field_numbers.Add(GetSpecificsFieldNumberFromModelType(model_type));
  }
  EXPECT_EQ(GetModelTypeSetFromSpecificsFieldNumberList(field_numbers),
            ProtocolTypes());
}

TEST(ModelTypeTest, TypesRequiringUnsyncedDataCheckOnSignout) {
  static_assert(
      51 == GetNumModelTypes(),
      "Add new types to `TypesRequiringUnsyncedDataCheckOnSignout()` if there "
      "should be a warning when the user signs out and the types have unsynced "
      "data. The warning offers the user to either save the data locally or "
      "abort sign-out, depending on the platform");

  EXPECT_TRUE(
      TypesRequiringUnsyncedDataCheckOnSignout().Has(syncer::PASSWORDS));
}

}  // namespace
}  // namespace syncer
