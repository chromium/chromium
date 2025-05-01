// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_proto_decoders.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyProtoDecodersTest : public testing::Test {
 public:
  PolicyMap policy_map_;
  PolicyMap expected_policy_map_;
  UserPolicyBuilder user_policy_;

  base::WeakPtr<CloudExternalDataManager> external_data_manager_;
};

TEST_F(PolicyProtoDecodersTest, BooleanPolicy) {
  expected_policy_map_.Set(key::kSearchSuggestEnabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(true), nullptr);

  user_policy_.payload().mutable_searchsuggestenabled()->set_value(true);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, IntegerPolicy) {
  expected_policy_map_.Set(key::kIncognitoModeAvailability,
                           POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD, base::Value(1), nullptr);

  user_policy_.payload().mutable_incognitomodeavailability()->set_value(1);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, StringPolicy) {
  expected_policy_map_.Set(key::kDefaultSearchProviderName,
                           POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD, base::Value("Google"), nullptr);

  user_policy_.payload().mutable_defaultsearchprovidername()->set_value(
      "Google");

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, StringListPolicy) {
  base::Value::List expected_disabled_sync_types;
  expected_disabled_sync_types.Append("bookmarks");
  expected_disabled_sync_types.Append("readingList");
  expected_policy_map_.Set(key::kSyncTypesListDisabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(std::move(expected_disabled_sync_types)),
                           nullptr);

  auto* disabled_sync_types =
      user_policy_.payload().mutable_synctypeslistdisabled()->mutable_value();
  disabled_sync_types->add_entries("bookmarks");
  disabled_sync_types->add_entries("readingList");

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, PolicyWithOptionUnset) {
  user_policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  user_policy_.payload()
      .mutable_searchsuggestenabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::UNSET);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  // Any values with PolicyOptions::UNSET will never set into policy_map_
  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, PolicyWithOptionRecommended) {
  expected_policy_map_.Set(key::kSearchSuggestEnabled, POLICY_LEVEL_RECOMMENDED,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(true), nullptr);

  user_policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  user_policy_.payload()
      .mutable_searchsuggestenabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, IntegerPolicyWithValueLowerThanMinLimit) {
  std::string too_small_value =
      base::NumberToString(std::numeric_limits<int32_t>::min() - 1LL);

  expected_policy_map_.Set(key::kIncognitoModeAvailability,
                           POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD, base::Value(too_small_value),
                           nullptr);
  expected_policy_map_.AddMessage(
      key::kIncognitoModeAvailability, PolicyMap::MessageType::kError,
      IDS_POLICY_PROTO_PARSING_ERROR, {u"Number out of range - invalid int32"});

  user_policy_.payload().mutable_incognitomodeavailability()->set_value(
      std::numeric_limits<int32_t>::min() - 1LL);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, IntegerPolicyWithValueUpperThanMaxLimit) {
  std::string too_big_value =
      base::NumberToString(std::numeric_limits<int32_t>::max() + 1LL);

  expected_policy_map_.Set(key::kIncognitoModeAvailability,
                           POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD, base::Value(too_big_value),
                           nullptr);
  expected_policy_map_.AddMessage(
      key::kIncognitoModeAvailability, PolicyMap::MessageType::kError,
      IDS_POLICY_PROTO_PARSING_ERROR, {u"Number out of range - invalid int32"});

  user_policy_.payload().mutable_incognitomodeavailability()->set_value(
      std::numeric_limits<int32_t>::max() + 1LL);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, JsonPolicy) {
  base::Value::Dict jsonPolicy;
  jsonPolicy.Set("key", "value");

  expected_policy_map_.Set(key::kManagedBookmarks, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(std::move(jsonPolicy)), nullptr);

  std::string jsonPolicyStr = R"({
    "key": "value"
    })";
  auto* disabled_managed_bookmarks_settings =
      user_policy_.payload().mutable_managedbookmarks()->mutable_value();
  disabled_managed_bookmarks_settings->append(jsonPolicyStr);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, InvalidJsonPolicy) {
  std::string invalidDummyJson = R"({
    "key": "value"
  )";  // lacks a close brace

  expected_policy_map_.Set(key::kManagedBookmarks, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(invalidDummyJson), nullptr);
  std::u16string kExpectedMessage =
      base::JSONReader::UsingRust()
          ? u"EOF while parsing an object at line 3 column 2"
          : u"Line: 3, column: 3, Syntax error.";
  expected_policy_map_.AddMessage(
      key::kManagedBookmarks, PolicyMap::MessageType::kError,
      IDS_POLICY_PROTO_PARSING_ERROR, {kExpectedMessage});

  auto* disabled_managed_bookmarks_settings =
      user_policy_.payload().mutable_managedbookmarks()->mutable_value();
  disabled_managed_bookmarks_settings->append(invalidDummyJson);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, PolicyWithAnyFilter) {
  expected_policy_map_.Set(key::kSearchSuggestEnabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(true), nullptr);
  expected_policy_map_.Set(key::kCloudReportingEnabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(true), nullptr);

  user_policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  user_policy_.payload().mutable_cloudreportingenabled()->set_value(true);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kAny);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, PolicyWithTrueFilter) {
  expected_policy_map_.Set(key::kSearchSuggestEnabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(true), nullptr);

  user_policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  user_policy_.payload().mutable_cloudreportingenabled()->set_value(true);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kTrue);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, PolicyWithFalseFilter) {
  expected_policy_map_.Set(key::kCloudReportingEnabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(true), nullptr);

  user_policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  user_policy_.payload().mutable_cloudreportingenabled()->set_value(true);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    PolicyPerProfileFilter::kFalse);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

}  // namespace policy
