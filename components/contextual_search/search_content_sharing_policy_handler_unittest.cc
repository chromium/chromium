// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/search_content_sharing_policy_handler.h"

#include "base/values.h"
#include "components/contextual_search/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_search {

class SearchContentSharingPolicyHandlerTest : public testing::Test {
 protected:
  void SetUp() override {}

  policy::PolicyMap policy_map_;
  PrefValueMap prefs_;
};

TEST_F(SearchContentSharingPolicyHandlerTest, NonBooleanConversion) {
  SearchContentSharingPolicyHandler handler(
      "test.pref.non_boolean",
      /* convert_policy_value_to_enabled_boolean= */ false);
  policy_map_.Set(policy::key::kSearchContentSharingSettings,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  handler.ApplyPolicySettings(policy_map_, &prefs_);

  const base::Value* value;
  EXPECT_TRUE(prefs_.GetValue("test.pref.non_boolean", &value));
  EXPECT_EQ(base::Value(1), *value);

  const base::Value* content_value;
  EXPECT_TRUE(prefs_.GetValue(kSearchContentSharingSettings, &content_value));
  EXPECT_EQ(base::Value(1), *content_value);
}

TEST_F(SearchContentSharingPolicyHandlerTest, BooleanConversion_Enabled) {
  SearchContentSharingPolicyHandler handler(
      "test.pref.boolean_enabled",
      /* convert_policy_value_to_enabled_boolean= */ true);
  policy_map_.Set(policy::key::kSearchContentSharingSettings,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  handler.ApplyPolicySettings(policy_map_, &prefs_);

  const base::Value* value;
  EXPECT_TRUE(prefs_.GetValue("test.pref.boolean_enabled", &value));
  EXPECT_EQ(base::Value(true), *value);

  const base::Value* content_value;
  EXPECT_TRUE(prefs_.GetValue(kSearchContentSharingSettings, &content_value));
  EXPECT_EQ(base::Value(0), *content_value);
}

TEST_F(SearchContentSharingPolicyHandlerTest, BooleanConversion_Disabled) {
  SearchContentSharingPolicyHandler handler(
      "test.pref.boolean_disabled",
      /* convert_policy_value_to_enabled_boolean= */ true);
  policy_map_.Set(policy::key::kSearchContentSharingSettings,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  handler.ApplyPolicySettings(policy_map_, &prefs_);

  const base::Value* value;
  EXPECT_TRUE(prefs_.GetValue("test.pref.boolean_disabled", &value));
  EXPECT_EQ(base::Value(false), *value);

  const base::Value* content_value;
  EXPECT_TRUE(prefs_.GetValue(kSearchContentSharingSettings, &content_value));
  EXPECT_EQ(base::Value(1), *content_value);
}

TEST_F(SearchContentSharingPolicyHandlerTest, PolicyNotSet) {
  SearchContentSharingPolicyHandler handler(
      "test.pref", /* convert_policy_value_to_enabled_boolean= */ false);
  handler.ApplyPolicySettings(policy_map_, &prefs_);
  EXPECT_TRUE(prefs_.empty());
}

}  // namespace contextual_search
