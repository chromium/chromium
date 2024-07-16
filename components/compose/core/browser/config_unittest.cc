// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/config.h"

#include "base/test/scoped_feature_list.h"
#include "components/compose/core/browser/compose_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ConfigTest : public testing::Test {
 public:
  void SetUp() override { compose::ResetConfigForTesting(); }

  void TearDown() override {
    scoped_feature_list_.Reset();
    compose::ResetConfigForTesting();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ConfigTest, ConfigUsesDefaultCountryValues) {
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.enabled_countries,
              testing::UnorderedElementsAre("bd", "ca", "gh", "in", "ke", "my",
                                            "ng", "ph", "pk", "sg", "tz", "ug",
                                            "us", "zm", "zw"));
}

TEST_F(ConfigTest, ConfigUsesCountryFinchValues) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      compose::features::kEnableCompose,
      {{"enabled_countries", " a,b c\td'e\"f\ng "}});
  compose::ResetConfigForTesting();
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.enabled_countries,
              testing::UnorderedElementsAre("a", "b", "c", "d", "e", "f", "g"));
}

TEST_F(ConfigTest, ConfigFallbackToDefaultsCountriesIfBadFinchValues) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      compose::features::kEnableCompose,
      {{"enabled_countries", ", \t' \n ,\" ,\"\t\n"}});
  compose::ResetConfigForTesting();
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.enabled_countries,
              testing::UnorderedElementsAre("bd", "ca", "gh", "in", "ke", "my",
                                            "ng", "ph", "pk", "sg", "tz", "ug",
                                            "us", "zm", "zw"));
}
