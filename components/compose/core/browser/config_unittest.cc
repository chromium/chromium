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

TEST_F(ConfigTest, ConfigUsesDefaultEnabledCountries) {
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.enabled_countries,
              testing::UnorderedElementsAre("bd", "ca", "gh", "in", "ke", "my",
                                            "ng", "ph", "pk", "sg", "tz", "ug",
                                            "us", "zm", "zw"));
}

TEST_F(ConfigTest, ConfigUsesEnabledCountryFinchValues) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      compose::features::kEnableCompose,
      {{"enabled_countries", " a,b c\td'e\"f\ng "}});
  compose::ResetConfigForTesting();
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.enabled_countries,
              testing::UnorderedElementsAre("a", "b", "c", "d", "e", "f", "g"));
}

TEST_F(ConfigTest, ConfigFallbackToDefaultEnabledCountriesOnEmptyList) {
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

TEST_F(ConfigTest, ConfigUsesDefaultProactiveNudgeCountries) {
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.proactive_nudge_countries,
              testing::UnorderedElementsAre("us"));
}

TEST_F(ConfigTest, ConfigUsesProactiveNudgeCountryFinchValues) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      compose::features::kEnableComposeProactiveNudge,
      {{"proactive_nudge_countries", " a,b c\td'e\"f\ng "}});
  compose::ResetConfigForTesting();
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.proactive_nudge_countries,
              testing::UnorderedElementsAre("a", "b", "c", "d", "e", "f", "g"));
}

TEST_F(ConfigTest, ConfigFallbackToDefaultProactiveNudgeCountriesOnEmptyList) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      compose::features::kEnableComposeProactiveNudge,
      {{"proactive_nudge_countries", ", \t' \n ,\" ,\"\t\n"}});
  compose::ResetConfigForTesting();
  compose::Config config = compose::GetComposeConfig();
  EXPECT_THAT(config.proactive_nudge_countries,
              testing::UnorderedElementsAre("us"));
}

TEST_F(ConfigTest, ConfigUnsignedIntNegativeParams) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{compose::features::kComposeInputParams,
        {{"min_words", "-200"}, {"max_words", "-1"}, {"max_chars", "-999999"}}},
       {compose::features::kComposeInnerText,
        {{"inner_text_max_bytes", "-200"},
         {"trimmed_inner_text_max_chars", "-1"},
         {"trimmed_inner_text_header_length", "-999999"}}},
       {compose::features::kEnableComposeProactiveNudge,
        {{"nudge_field_change_event_max", "-200"}}}},
      {});

  compose::ResetConfigForTesting();
  compose::Config config = compose::GetComposeConfig();
  EXPECT_EQ(config.input_min_words, 0u);
  EXPECT_EQ(config.input_max_words, 0u);
  EXPECT_EQ(config.input_max_chars, 0u);
  EXPECT_EQ(config.inner_text_max_bytes, 0u);
  EXPECT_EQ(config.trimmed_inner_text_max_chars, 0u);
  EXPECT_EQ(config.trimmed_inner_text_header_length, 0u);
  EXPECT_EQ(config.nudge_field_change_event_max, 0u);
}

TEST_F(ConfigTest, ConfigUnsignedIntPositiveAndDefaultParams) {
  compose::Config config = compose::GetComposeConfig();
  unsigned int default_min_words = config.input_min_words;
  unsigned int default_inner_text_max_bytes = config.inner_text_max_bytes;
  unsigned int default_nudge_field_change_event_max =
      config.nudge_field_change_event_max;

  // Do not overwrite input_min_words, inner_test_max_bytes, or
  // nudge_field_change_event_max
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{compose::features::kComposeInputParams,
        {{"max_words", "123"}, {"max_chars", "321"}}},
       {compose::features::kComposeInnerText,
        {{"trimmed_inner_text_max_chars", "789"},
         {"trimmed_inner_text_header_length", "987"}}}},
      {});

  compose::ResetConfigForTesting();
  config = compose::GetComposeConfig();
  EXPECT_EQ(config.input_min_words, default_min_words);
  EXPECT_EQ(config.input_max_words, 123u);
  EXPECT_EQ(config.input_max_chars, 321u);
  EXPECT_EQ(config.inner_text_max_bytes, default_inner_text_max_bytes);
  EXPECT_EQ(config.trimmed_inner_text_max_chars, 789u);
  EXPECT_EQ(config.trimmed_inner_text_header_length, 987u);
  EXPECT_EQ(config.nudge_field_change_event_max,
            default_nudge_field_change_event_max);
}
