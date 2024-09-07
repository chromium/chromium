// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingApiHandlerUtilTest : public ::testing::Test {};

TEST_F(SafeBrowsingApiHandlerUtilTest, GetThreatMetadataFromSafeBrowsingApi) {
  typedef SubresourceFilterLevel Level;
  typedef SubresourceFilterType Type;
  const struct {
    SafeBrowsingJavaThreatType threat_type;
    std::vector<int> threat_attributes;
    SubresourceFilterMatch expected_match;
  } test_cases[] = {
      {SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING, {}, {}},
      {SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       {},
       {{Type::ABUSIVE, Level::ENFORCE}}},
      {SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       {static_cast<int>(SafeBrowsingJavaThreatAttribute::CANARY)},
       {{Type::ABUSIVE, Level::WARN}}},
      {SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
       {static_cast<int>(SafeBrowsingJavaThreatAttribute::FRAME_ONLY)},
       {{Type::ABUSIVE, Level::ENFORCE}}},
      {SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION,
       {},
       {{Type::BETTER_ADS, Level::ENFORCE}}},
      {SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION,
       {static_cast<int>(SafeBrowsingJavaThreatAttribute::CANARY)},
       {{Type::BETTER_ADS, Level::WARN}}},
      {SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION,
       {static_cast<int>(SafeBrowsingJavaThreatAttribute::FRAME_ONLY)},
       {{Type::BETTER_ADS, Level::ENFORCE}}}};

  for (const auto& test_case : test_cases) {
    ThreatMetadata metadata = GetThreatMetadataFromSafeBrowsingApi(
        test_case.threat_type, test_case.threat_attributes);
    ThreatMetadata expected;
    expected.subresource_filter_match = test_case.expected_match;
    EXPECT_EQ(expected, metadata);
  }
}

}  // namespace safe_browsing
