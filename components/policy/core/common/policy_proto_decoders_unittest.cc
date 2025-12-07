// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_proto_decoders.h"

#include "base/strings/string_number_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kExtensionId1[] = "extension_id_1";
constexpr char kExtensionId2[] = "extension_id_2";
constexpr char kExtensionId3[] = "extension_id_3";
constexpr char kExtension1Version1[] = "1.0.1.1";
constexpr char kExtension1Version2[] = "2.0.1.1";
constexpr char kExtension2Version2[] = "2.0.2.2";
constexpr char kExtension3Version3[] = "3.0.3.3";
}  // namespace

class PolicyProtoDecodersTest : public testing::Test {
 public:
  PolicyMap policy_map_;
  PolicyMap expected_policy_map_;
  UserPolicyBuilder user_policy_;
  ExtensionInstallPoliciesBuilder extension_install_policies_;

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
  const std::u16string kExpectedMessage =
      u"EOF while parsing an object at line 3 column 2";
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

TEST_F(PolicyProtoDecodersTest, ExtensionInstallPolicies) {
  base::Value::Dict expected_policy_value1;
  base::Value::Dict& version_dict =
      expected_policy_value1.Set(kExtension1Version1, base::Value::Dict())
          ->GetDict();
  version_dict.Set("action", 0);
  version_dict.Set("reasons", base::Value::List());

  base::Value::Dict expected_policy_value2;
  base::Value::Dict& version_dict2 =
      expected_policy_value2.Set(kExtension2Version2, base::Value::Dict())
          ->GetDict();
  version_dict2.Set("action", 1);
  version_dict2.Set("reasons", base::Value::List());

  base::Value::Dict expected_policy_value3;
  base::Value::Dict& version_dict3 =
      expected_policy_value3.Set(kExtension3Version3, base::Value::Dict())
          ->GetDict();
  version_dict3.Set("action", 2);
  base::Value::List expected_reasons;
  expected_reasons.Append(1);
  expected_reasons.Append(2);
  version_dict3.Set("reasons", std::move(expected_reasons));

  base::Value::Dict& version_dict4 =
      expected_policy_value1.Set(kExtension1Version2, base::Value::Dict())
          ->GetDict();
  version_dict4.Set("action", 0);
  version_dict4.Set("reasons", base::Value::List());

  expected_policy_map_.Set(kExtensionId1, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(std::move(expected_policy_value1)),
                           nullptr);
  expected_policy_map_.Set(kExtensionId2, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(std::move(expected_policy_value2)),
                           nullptr);
  expected_policy_map_.Set(kExtensionId3, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(std::move(expected_policy_value3)),
                           nullptr);

  {
    em::ExtensionInstallPolicy* policy =
        extension_install_policies_.payload().add_policies();
    policy->set_extension_id(kExtensionId1);
    policy->set_extension_version(kExtension1Version1);
    policy->set_action(em::ExtensionInstallPolicy::ACTION_UNSPECIFIED);
  }
  {
    em::ExtensionInstallPolicy* policy =
        extension_install_policies_.payload().add_policies();
    policy->set_extension_id(kExtensionId2);
    policy->set_extension_version(kExtension2Version2);
    policy->set_action(em::ExtensionInstallPolicy::ACTION_ALLOW);
  }
  {
    em::ExtensionInstallPolicy* policy =
        extension_install_policies_.payload().add_policies();
    policy->set_extension_id(kExtensionId3);
    policy->set_extension_version(kExtension3Version3);
    policy->set_action(em::ExtensionInstallPolicy::ACTION_BLOCK);
    policy->add_reasons(em::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY);
    policy->add_reasons(em::ExtensionInstallPolicy::REASON_RISK_SCORE);
  }
  {
    em::ExtensionInstallPolicy* policy =
        extension_install_policies_.payload().add_policies();
    policy->set_extension_id(kExtensionId1);
    policy->set_extension_version(kExtension1Version2);
    policy->set_action(em::ExtensionInstallPolicy::ACTION_UNSPECIFIED);
  }
  DecodeProtoFields(extension_install_policies_.payload(), POLICY_SOURCE_CLOUD,
                    POLICY_SCOPE_USER, &policy_map_);
  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, ExtensionInstallPoliciesMalformedNotSet) {
  // Policy with no extension id should be ignored.
  {
    em::ExtensionInstallPolicy* policy =
        extension_install_policies_.payload().add_policies();
    policy->set_extension_version(kExtension1Version1);
    policy->set_action(em::ExtensionInstallPolicy::ACTION_UNSPECIFIED);
  }
  // Policy with no extension version should be ignored.
  {
    em::ExtensionInstallPolicy* policy =
        extension_install_policies_.payload().add_policies();
    policy->set_extension_id(kExtensionId2);
    policy->set_action(em::ExtensionInstallPolicy::ACTION_ALLOW);
  }

  // Policy with no extension id and version should be ignored.
  {
    em::ExtensionInstallPolicy* policy =
        extension_install_policies_.payload().add_policies();
    policy->set_action(em::ExtensionInstallPolicy::ACTION_BLOCK);
    policy->add_reasons(em::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY);
    policy->add_reasons(em::ExtensionInstallPolicy::REASON_RISK_SCORE);
  }
  DecodeProtoFields(extension_install_policies_.payload(), POLICY_SOURCE_CLOUD,
                    POLICY_SCOPE_USER, &policy_map_);
  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}
}  // namespace policy
