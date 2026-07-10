// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_protection/utils.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/enterprise/data_protection/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_data_protection {

namespace {

void AddDummyMatchedRule(safe_browsing::RTLookupResponse& rt_lookup_response,
                         const char* watermark_text,
                         bool allow_screenshot) {
  int count = rt_lookup_response.threat_info().size();
  auto* threat_info = rt_lookup_response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id(
      base::StringPrintf("test rule id-%d", count));
  matched_url_navigation_rule->set_rule_name(
      base::StringPrintf("test rule name-%d", count));
  matched_url_navigation_rule->set_matched_url_category(
      base::StringPrintf("test rule category-%d", count));
  if (watermark_text && watermark_text[0] != '\0') {
    matched_url_navigation_rule->mutable_watermark_message()
        ->set_watermark_message(watermark_text);
  }
  matched_url_navigation_rule->set_block_screenshot(!allow_screenshot);
}

std::unique_ptr<safe_browsing::RTLookupResponse> BuildDummyResponse(
    const char* watermark_text,
    bool allow_screenshot) {
  auto rt_lookup_response = std::make_unique<safe_browsing::RTLookupResponse>();
  AddDummyMatchedRule(*rt_lookup_response, watermark_text, allow_screenshot);
  return rt_lookup_response;
}

struct WatermarkFormatTestCase {
  const char* timezone;
  int64_t seconds;
  int32_t nanos = 0;
  const char* custom_message = "";
  const char* identifier = "user@example.com";
  const char* expected_text = "";
};

}  // namespace

class DataProtectionUtilsTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DataProtectionUtilsTest, GetUrlSettings_NoResponse) {
  UrlSettings settings = GetUrlSettings("identifier", nullptr);

  EXPECT_TRUE(settings.allow_screenshots);
  EXPECT_TRUE(settings.watermark_text.empty());
}

TEST_F(DataProtectionUtilsTest, GetUrlSettings_EmptyResponse) {
  auto rt_lookup_response = std::make_unique<safe_browsing::RTLookupResponse>();

  UrlSettings settings = GetUrlSettings("identifier", rt_lookup_response.get());

  EXPECT_TRUE(settings.allow_screenshots);
  EXPECT_TRUE(settings.watermark_text.empty());
}

TEST_F(DataProtectionUtilsTest, GetUrlSettings_RT_Blocks) {
  auto rt_lookup_response = BuildDummyResponse("rt_watermark", false);

  UrlSettings settings = GetUrlSettings("identifier", rt_lookup_response.get());

  EXPECT_FALSE(settings.allow_screenshots);
  EXPECT_FALSE(settings.watermark_text.empty());
  EXPECT_NE(settings.watermark_text.find("rt_watermark"), std::string::npos);
  EXPECT_NE(settings.watermark_text.find("identifier"), std::string::npos);
}

TEST_F(DataProtectionUtilsTest, GetUrlSettings_MultipleRules) {
  auto rt_lookup_response = std::make_unique<safe_browsing::RTLookupResponse>();
  AddDummyMatchedRule(*rt_lookup_response, "first_watermark", true);
  AddDummyMatchedRule(*rt_lookup_response, "second_watermark", false);

  UrlSettings settings = GetUrlSettings("identifier", rt_lookup_response.get());

  EXPECT_FALSE(settings.allow_screenshots);
  // Should take first watermark found
  EXPECT_NE(settings.watermark_text.find("first_watermark"), std::string::npos);
  EXPECT_EQ(settings.watermark_text.find("second_watermark"),
            std::string::npos);
}

TEST_F(DataProtectionUtilsTest, GetWatermarkString_NoMessage) {
  safe_browsing::MatchedUrlNavigationRule rule;
  EXPECT_TRUE(GetWatermarkString("identifier", rule).empty());
}

TEST_F(DataProtectionUtilsTest, GetWatermarkString_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableWatermarkTimestampTimezone);

  safe_browsing::MatchedUrlNavigationRule rule;
  auto* watermark = rule.mutable_watermark_message();
  watermark->set_watermark_message("Confidential");
  watermark->mutable_timestamp()->set_seconds(1700000000);
  watermark->mutable_timestamp()->set_nanos(0);

  std::string result = GetWatermarkString("user@example.com", rule);
  EXPECT_EQ(result, "Confidential\nuser@example.com\n2023-11-14T22:13:20.000Z");
}

class DataProtectionUtilsTimezoneTest
    : public testing::TestWithParam<WatermarkFormatTestCase> {
 protected:
  DataProtectionUtilsTimezoneTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnableWatermarkTimestampTimezone);
  }
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(DataProtectionUtilsTimezoneTest, FormatsTimestamp) {
  const auto& param = GetParam();
  base::test::ScopedRestoreDefaultTimezone tz(param.timezone);

  safe_browsing::MatchedUrlNavigationRule rule;
  auto* watermark = rule.mutable_watermark_message();
  watermark->set_watermark_message(param.custom_message);
  watermark->mutable_timestamp()->set_seconds(param.seconds);
  watermark->mutable_timestamp()->set_nanos(param.nanos);

  EXPECT_EQ(GetWatermarkString(param.identifier, rule), param.expected_text);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DataProtectionUtilsTimezoneTest,
    testing::ValuesIn(std::vector<WatermarkFormatTestCase>{
        {"UTC", 1700000000, 0, "Confidential", "user@example.com",
         "Confidential\nuser@example.com\n2023-11-14 22:13:20 (UTC+00:00)"},
        {"Asia/Tokyo", 1700000000, 0, "Internal Use Only", "user@example.com",
         "Internal Use Only\nuser@example.com\n2023-11-15 07:13:20 "
         "(UTC+09:00)"},
        {"America/Toronto", 1700000000, 0, "Do Not Copy", "user@example.com",
         "Do Not Copy\nuser@example.com\n2023-11-14 17:13:20 (UTC-05:00)"},
        {"Asia/Kolkata", 1700000000, 0, "Restricted", "user@example.com",
         "Restricted\nuser@example.com\n2023-11-15 03:43:20 (UTC+05:30)"},
        {"Asia/Kathmandu", 1705000000, 0, "", "user@example.com",
         "user@example.com\n2024-01-12 00:51:40 (UTC+05:45)"},
        {"UTC", 1700000000, 0, "", "user@example.com",
         "user@example.com\n2023-11-14 22:13:20 (UTC+00:00)"},
        {"America/Los_Angeles", 0, 0, "", "user@example.com",
         "user@example.com\n1969-12-31 16:00:00 (UTC-08:00)"},
        {"Africa/Casablanca", 1709249400, 0, "", "user@example.com",
         "user@example.com\n2024-03-01 00:30:00 (UTC+01:00)"},
        {"America/Los_Angeles", 1710064800, 0, "", "user@example.com",
         "user@example.com\n2024-03-10 03:00:00 (UTC-07:00)"},
        {"Asia/Tokyo", 1700000000, 999999000, "Subsecond Test",
         "user@example.com",
         "Subsecond Test\nuser@example.com\n2023-11-15 07:13:20 (UTC+09:00)"},
    }));

}  // namespace enterprise_data_protection
