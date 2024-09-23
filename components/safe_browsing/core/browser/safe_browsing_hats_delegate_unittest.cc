// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingHatsDelegateTest : public ::testing::Test {
 public:
  static constexpr char kAllReportTypeFilters[] =
      "URL_PHISHING,URL_MALWARE,URL_UNWANTED,URL_CLIENT_SIDE_PHISHING";
  static constexpr char kAllDidProceedFilters[] = "TRUE,FALSE";

 protected:
  using enum SBThreatType;
};

TEST_F(SafeBrowsingHatsDelegateTest, IsSurveyCandidateWithAllFilters) {
  // Validate all supported report types are included.
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_PHISHING, kAllReportTypeFilters, true,
      kAllDidProceedFilters));
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_MALWARE, kAllReportTypeFilters, false,
      kAllDidProceedFilters));
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_UNWANTED, kAllReportTypeFilters, true,
      kAllDidProceedFilters));
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING, kAllReportTypeFilters, false,
      kAllDidProceedFilters));
  // Validate unsupported SB threat types are excluded.
  EXPECT_FALSE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_SAVED_PASSWORD_REUSE, kAllReportTypeFilters, true,
      kAllDidProceedFilters));
  EXPECT_FALSE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_AD_SAMPLE, kAllReportTypeFilters, false,
      kAllDidProceedFilters));
}

TEST_F(SafeBrowsingHatsDelegateTest, IsSurveyCandidateWithSomeFilters) {
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_MALWARE, kAllReportTypeFilters, false, "FALSE"));
  EXPECT_FALSE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_MALWARE, kAllReportTypeFilters, false, "TRUE"));
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_MALWARE, "URL_MALWARE", true, "TRUE"));
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_MALWARE, "URL_MALWARE,URL_PHISHING", true, "TRUE"));
  EXPECT_TRUE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_PHISHING, "URL_MALWARE,URL_PHISHING", true, "TRUE"));
  EXPECT_FALSE(SafeBrowsingHatsDelegate::IsSurveyCandidate(
      SB_THREAT_TYPE_URL_PHISHING, "URL_UNWANTED", true, "TRUE"));
}

}  // namespace safe_browsing
