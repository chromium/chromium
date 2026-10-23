// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/settings_ui_registry.h"

#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class SettingsUiRegistryTest : public testing::Test {
 public:
  SettingsUiRegistryTest() = default;
  ~SettingsUiRegistryTest() override = default;

  void TearDown() override { registry_.ClearForTesting(); }

 protected:
  SettingsUiRegistry registry_;
};

TEST_F(SettingsUiRegistryTest, Register) {
  EnterprisePolicyPref enterprise_policy("policy_name");
  auto metadata = std::make_unique<SettingsUiMetadata>(
      "Test", UserVisibleFeatureKey::kCompose, enterprise_policy);
  registry_.Register(std::move(metadata));

  const auto* metadata_from_registry =
      registry_.GetFeature(UserVisibleFeatureKey::kCompose);
  EXPECT_TRUE(metadata_from_registry);
  EXPECT_EQ("Test", metadata_from_registry->name());
  EXPECT_EQ("policy_name", metadata_from_registry->enterprise_policy().name());
  EXPECT_FALSE(registry_.GetFeature(UserVisibleFeatureKey::kWallpaperSearch));
}

}  // namespace optimization_guide
