// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/survey_config.h"

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
