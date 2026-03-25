// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_protection/utils.h"

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
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

}  // namespace enterprise_data_protection
