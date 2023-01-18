// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"

#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(HashRealTimeUtilsTest, TestGetHashPrefix) {
  EXPECT_EQ(
      hash_realtime_utils::GetHashPrefix("abcd1111111111111111111111111111"),
      "abcd");
  EXPECT_EQ(
      hash_realtime_utils::GetHashPrefix("dcba1111111111111111111111111111"),
      "dcba");
}

TEST(HashRealTimeUtilsTest, TestIsThreatTypeRelevant) {
  EXPECT_TRUE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::MALWARE));
  EXPECT_TRUE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::SOCIAL_ENGINEERING));
  EXPECT_TRUE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::UNWANTED_SOFTWARE));
  EXPECT_TRUE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::SUSPICIOUS));
  EXPECT_TRUE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::TRICK_TO_BILL));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION));
  EXPECT_FALSE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::API_ABUSE));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::SOCIAL_ENGINEERING_ADS));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::BETTER_ADS_VIOLATION));
}

}  // namespace safe_browsing
