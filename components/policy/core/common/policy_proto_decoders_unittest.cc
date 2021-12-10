// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_proto_decoders.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
const PolicyPerProfileFilter kFilter = PolicyPerProfileFilter::kAny;
}

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
                    kFilter);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, IntegerPolicy) {
  expected_policy_map_.Set(key::kIncognitoModeAvailability,
                           POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD, base::Value(1), nullptr);

  user_policy_.payload().mutable_incognitomodeavailability()->set_value(1);

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    kFilter);

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
                    kFilter);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

TEST_F(PolicyProtoDecodersTest, StringListPolicy) {
  std::vector<base::Value> expected_disabled_sync_types;
  expected_disabled_sync_types.emplace_back(base::Value("bookmarks"));
  expected_disabled_sync_types.emplace_back(base::Value("readingList"));
  expected_policy_map_.Set(key::kSyncTypesListDisabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           base::Value(expected_disabled_sync_types), nullptr);

  auto* disabled_sync_types =
      user_policy_.payload().mutable_synctypeslistdisabled()->mutable_value();
  disabled_sync_types->add_entries("bookmarks");
  disabled_sync_types->add_entries("readingList");

  DecodeProtoFields(user_policy_.payload(), external_data_manager_,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map_,
                    kFilter);

  EXPECT_TRUE(expected_policy_map_.Equals(policy_map_));
}

// TODO(crbug.com/1278735): Add more tests cases for DecodeProtoFields

}  // namespace policy
