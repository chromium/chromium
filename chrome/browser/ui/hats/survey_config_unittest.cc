// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/survey_config.h"

#include <vector>

#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SurveyConfigTest : public testing::Test {
 public:
  SurveyConfigTest() = default;
};

TEST_F(SurveyConfigTest, ValidateHatsHistogramName) {
  EXPECT_EQ(std::nullopt,
            hats::SurveyConfig::ValidateHatsHistogramName(std::nullopt));
  EXPECT_EQ(std::nullopt, hats::SurveyConfig::ValidateHatsHistogramName(""));
  EXPECT_EQ(std::nullopt,
            hats::SurveyConfig::ValidateHatsHistogramName("ExampleSurvey"));

  EXPECT_EQ(std::make_optional<std::string>(
                "Feedback.HappinessTrackingSurvey.ExampleSurvey"),
            hats::SurveyConfig::ValidateHatsHistogramName(
                "Feedback.HappinessTrackingSurvey.ExampleSurvey"));
}

TEST_F(SurveyConfigTest, ValidateHatsSurveyUkmId) {
  EXPECT_EQ(std::nullopt, hats::SurveyConfig::ValidateHatsSurveyUkmId(0));
  EXPECT_EQ(std::nullopt,
            hats::SurveyConfig::ValidateHatsSurveyUkmId(std::nullopt));

  EXPECT_EQ(std::make_optional<uint64_t>(1),
            hats::SurveyConfig::ValidateHatsSurveyUkmId(1));
}

// HaTS on Desktop and Android adds the Chrome channel and version to every
// survey's Product Specific Data, so surveys must not declare fields with the
// same keys.
TEST_F(SurveyConfigTest, NoSurveyDeclaresDefaultProductSpecificDataKeys) {
  const auto is_default_key =
      testing::AnyOf(testing::Eq(HatsServiceDesktop::kChromeChannelPsdKey),
                     testing::Eq(HatsServiceDesktop::kChromeVersionPsdKey));

  const std::vector<hats::SurveyConfig> configs =
      hats::GetAllSurveyConfigsForTesting();
  ASSERT_FALSE(configs.empty());
  for (const hats::SurveyConfig& config : configs) {
    EXPECT_THAT(config.product_specific_bits_data_fields,
                testing::Not(testing::Contains(is_default_key)))
        << "Survey with trigger " << config.trigger;
    EXPECT_THAT(config.product_specific_string_data_fields,
                testing::Not(testing::Contains(is_default_key)))
        << "Survey with trigger " << config.trigger;
  }
}
