// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class MqlsFeatureRegistryTest : public testing::Test {
 public:
  MqlsFeatureRegistryTest() = default;
  ~MqlsFeatureRegistryTest() override = default;

  void TearDown() override {
    MqlsFeatureRegistry::GetInstance().ClearForTesting();
  }
};

BASE_FEATURE(kLoggingEnabledFeature,
             "LoggingEnabledFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

TEST_F(MqlsFeatureRegistryTest, RegisterFeature) {
  EnterprisePolicyPref enterprise_policy("policy_name");
  UserFeedbackCallback logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
      });
  auto metadata = std::make_unique<MqlsFeatureMetadata>(
      "Test", proto::LogAiDataRequest::FeatureCase::kCompose, enterprise_policy,
      &kLoggingEnabledFeature, logging_callback,
      UserVisibleFeatureKey::kCompose);
  MqlsFeatureRegistry::GetInstance().Register(std::move(metadata));

  const MqlsFeatureMetadata* metadata_from_registry =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_TRUE(metadata_from_registry);
  EXPECT_EQ("Test", metadata_from_registry->name());
  EXPECT_EQ("policy_name", metadata_from_registry->enterprise_policy().name());
  EXPECT_EQ(logging_callback,
            metadata_from_registry->get_user_feedback_callback());
  EXPECT_EQ(&kLoggingEnabledFeature,
            metadata_from_registry->field_trial_feature());
  EXPECT_EQ(UserVisibleFeatureKey::kCompose,
            metadata_from_registry->user_visible_feature_key());
  EXPECT_FALSE(MqlsFeatureRegistry::GetInstance().GetFeature(
      proto::LogAiDataRequest::FeatureCase::kTabOrganization));
}

TEST(MqlsFeatureMetadataTest, LoggingEnabledViaFieldTrial) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kLoggingEnabledFeature);

  UserFeedbackCallback logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
      });
  MqlsFeatureMetadata metadata(
      "Test", proto::LogAiDataRequest::FeatureCase::kCompose,
      EnterprisePolicyPref("policy_name"), &kLoggingEnabledFeature,
      logging_callback, std::nullopt);
  EXPECT_TRUE(metadata.LoggingEnabledViaFieldTrial());
}

TEST(MqlsFeatureMetadataTest, LoggingDisabledViaFieldTrial) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kLoggingEnabledFeature);

  UserFeedbackCallback logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
      });
  MqlsFeatureMetadata metadata(
      "Test", proto::LogAiDataRequest::FeatureCase::kCompose,
      EnterprisePolicyPref("policy_name"), &kLoggingEnabledFeature,
      logging_callback, std::nullopt);
  EXPECT_FALSE(metadata.LoggingEnabledViaFieldTrial());
}

}  // namespace optimization_guide
