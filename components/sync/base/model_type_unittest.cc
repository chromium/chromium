// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class ModelTypeTest : public testing::Test {};

TEST_F(ModelTypeTest, ModelTypeToValue) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < ModelType::NUM_ENTRIES; ++i) {
    ModelType model_type = ModelTypeFromInt(i);
    base::ExpectStringValue(ModelTypeToString(model_type),
                            *ModelTypeToValue(model_type));
  }
  base::ExpectStringValue("Top-level folder",
                          *ModelTypeToValue(TOP_LEVEL_FOLDER));
  base::ExpectStringValue("Unspecified", *ModelTypeToValue(UNSPECIFIED));
}

TEST_F(ModelTypeTest, ModelTypeSetToValue) {
  const ModelTypeSet model_types(BOOKMARKS, APPS);

  std::unique_ptr<base::ListValue> value(ModelTypeSetToValue(model_types));
  EXPECT_EQ(2u, value->GetSize());
  std::string types[2];
  EXPECT_TRUE(value->GetString(0, &types[0]));
  EXPECT_TRUE(value->GetString(1, &types[1]));
  EXPECT_EQ("Bookmarks", types[0]);
  EXPECT_EQ("Apps", types[1]);
}

TEST_F(ModelTypeTest, IsRealDataType) {
  EXPECT_FALSE(IsRealDataType(UNSPECIFIED));
  EXPECT_FALSE(IsRealDataType(ModelType::NUM_ENTRIES));
  EXPECT_FALSE(IsRealDataType(TOP_LEVEL_FOLDER));
  EXPECT_TRUE(IsRealDataType(FIRST_REAL_MODEL_TYPE));
  EXPECT_TRUE(IsRealDataType(BOOKMARKS));
  EXPECT_TRUE(IsRealDataType(APPS));
  EXPECT_TRUE(IsRealDataType(ARC_PACKAGE));
  EXPECT_TRUE(IsRealDataType(PRINTERS));
  EXPECT_TRUE(IsRealDataType(READING_LIST));
}

TEST_F(ModelTypeTest, IsProxyType) {
  EXPECT_FALSE(IsProxyType(BOOKMARKS));
  EXPECT_FALSE(IsProxyType(ModelType::NUM_ENTRIES));
  EXPECT_TRUE(IsProxyType(PROXY_TABS));
}

// Make sure we can convert ModelTypes to and from specifics field
// numbers.
TEST_F(ModelTypeTest, ModelTypeToFromSpecificsFieldNumber) {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    int field_number = GetSpecificsFieldNumberFromModelType(type);
    EXPECT_EQ(type, GetModelTypeFromSpecificsFieldNumber(field_number));
  }
}

TEST_F(ModelTypeTest, ModelTypeOfInvalidSpecificsFieldNumber) {
  EXPECT_EQ(UNSPECIFIED, GetModelTypeFromSpecificsFieldNumber(0));
}

TEST_F(ModelTypeTest, ModelTypeHistogramMapping) {
  std::set<ModelTypeForHistograms> histogram_values;
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType type : all_types) {
    SCOPED_TRACE(ModelTypeToString(type));
    ModelTypeForHistograms histogram_value = ModelTypeHistogramValue(type);

    EXPECT_TRUE(histogram_values.insert(histogram_value).second)
        << "Expected histogram values to be unique";

    EXPECT_LE(static_cast<int>(histogram_value),
              static_cast<int>(ModelTypeForHistograms::kMaxValue));
  }
}

TEST_F(ModelTypeTest, ModelTypeToStableIdentifier) {
  std::set<int> identifiers;
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType type : all_types) {
    SCOPED_TRACE(ModelTypeToString(type));
    int stable_identifier = ModelTypeToStableIdentifier(type);
    EXPECT_GT(stable_identifier, 0);
    EXPECT_TRUE(identifiers.insert(stable_identifier).second)
        << "Expected identifier values to be unique";
  }

  // Hard code a few example model_types to make it harder to break that the
  // identifiers are stable.
  EXPECT_EQ(3, ModelTypeToStableIdentifier(BOOKMARKS));
  EXPECT_EQ(7, ModelTypeToStableIdentifier(AUTOFILL));
  EXPECT_EQ(9, ModelTypeToStableIdentifier(TYPED_URLS));
}

TEST_F(ModelTypeTest, ModelTypeSetFromString) {
  ModelTypeSet empty;
  ModelTypeSet one(BOOKMARKS);
  ModelTypeSet two(BOOKMARKS, TYPED_URLS);

  EXPECT_EQ(empty, ModelTypeSetFromString(ModelTypeSetToString(empty)));
  EXPECT_EQ(one, ModelTypeSetFromString(ModelTypeSetToString(one)));
  EXPECT_EQ(two, ModelTypeSetFromString(ModelTypeSetToString(two)));
}

TEST_F(ModelTypeTest, DefaultFieldValues) {
  ModelTypeSet types = ProtocolTypes();
  for (ModelType type : types) {
    SCOPED_TRACE(ModelTypeToString(type));

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

TEST_F(ModelTypeTest, ModelTypeToRootTagValues) {
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType model_type : all_types) {
    std::string root_tag = ModelTypeToRootTag(model_type);
    if (IsProxyType(model_type)) {
      EXPECT_EQ(root_tag, std::string());
    } else if (IsRealDataType(model_type)) {
      EXPECT_TRUE(base::StartsWith(root_tag, "google_chrome_",
                                   base::CompareCase::INSENSITIVE_ASCII));
    } else {
      EXPECT_EQ(root_tag, "INVALID");
    }
  }
}

TEST_F(ModelTypeTest, ModelTypeStringMapping) {
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType model_type : all_types) {
    const char* model_type_string = ModelTypeToString(model_type);
    ModelType converted_model_type = ModelTypeFromString(model_type_string);
    if (IsRealDataType(model_type))
      EXPECT_EQ(converted_model_type, model_type);
    else
      EXPECT_EQ(converted_model_type, UNSPECIFIED);
  }
}

TEST_F(ModelTypeTest, ModelTypeNotificationTypeMapping) {
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelType model_type : all_types) {
    std::string notification_type;
    bool ret = RealModelTypeToNotificationType(model_type, &notification_type);
    if (ret) {
      ModelType notified_model_type;
      EXPECT_TRUE(NotificationTypeToRealModelType(notification_type,
                                                  &notified_model_type));
      EXPECT_EQ(notified_model_type, model_type);
    } else {
      EXPECT_FALSE(ProtocolTypes().Has(model_type));
      EXPECT_TRUE(notification_type.empty());
    }
  }
}

}  // namespace
}  // namespace syncer
