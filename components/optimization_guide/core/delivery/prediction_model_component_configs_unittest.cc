// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prediction_model_component_configs.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class PredictionModelComponentConfigsTest : public testing::Test {
 public:
  PredictionModelComponentConfigsTest() = default;
  ~PredictionModelComponentConfigsTest() override = default;
};

TEST_F(PredictionModelComponentConfigsTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kPredictionModelComponentDelivery);

  EXPECT_FALSE(GetPredictionModelComponentConfig(
                   proto::OPTIMIZATION_TARGET_MODEL_VALIDATION)
                   .has_value());
  EXPECT_TRUE(GetPredictionModelTargets().empty());
}

TEST_F(PredictionModelComponentConfigsTest, FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPredictionModelComponentDelivery);

  // Test supported target.
  auto config = GetPredictionModelComponentConfig(
      proto::OPTIMIZATION_TARGET_MODEL_VALIDATION);
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->component_name(), "Optimization Guide Model Validation");
  EXPECT_EQ(config->component_id(), "cfofaddefefcbblgcgnibnonglccbfja");

  // Check public key hash length.
  EXPECT_EQ(config->public_key_sha256().size(), 32u);

  // Test unsupported target.
  EXPECT_FALSE(GetPredictionModelComponentConfig(
                   proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());

  // Test target list.
  for (auto target : GetPredictionModelTargets()) {
    EXPECT_TRUE(GetPredictionModelComponentConfig(target).has_value())
        << "Missing config for target: " << target;
  }
}

}  // namespace optimization_guide
