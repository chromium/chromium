// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/recent_session_policy.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Arbitrary date not too far from the current date.
constexpr base::Time kNow =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(150000));

// Calculates a set of start times in terms of time before kNow.
// The most recent time will always be kNow; the other times are
// added after and should be in increasing order of time interval.
RecentSessionData CreateSessionData(
    const std::initializer_list<base::TimeDelta>& before_now,
    const std::optional<base::TimeDelta>& since_enabled) {
  RecentSessionData data;
  data.recent_session_start_times.push_back(kNow);
  for (const auto& delta : before_now) {
    data.recent_session_start_times.push_back(kNow - delta);
  }
  if (since_enabled) {
    data.enabled_time = kNow - *since_enabled;
  }
  return data;
}

}  // namespace

class RecentSessionPolicyTest : public testing::Test {
 public:
  RecentSessionPolicyTest() = default;
  ~RecentSessionPolicyTest() override = default;

 protected:
  base::HistogramTester histograms_;
  RecentSessionPolicyImpl policy_;
};

TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics) {
  const RecentSessionPolicyImpl::UsageThresholds thresholds = {
      {3, base::Days(7)}, {5, base::Days(21)}};
  policy_.set_thresholds_for_testing(thresholds);

  auto data =
      CreateSessionData({base::Days(1), base::Days(2), base::Days(3),
                         base::Days(10), base::Days(20), base::Days(30)},
                        base::Days(60));
  policy_.RecordRecentUsageMetrics(data);
  histograms_.ExpectBucketCount("UserEducation.Session.ShortTermCount", 4, 1);
  histograms_.ExpectBucketCount("UserEducation.Session.LongTermCount", 6, 1);

  // Advance by enough time to move elements between buckets.
  data.recent_session_start_times.insert(
      data.recent_session_start_times.begin(),
      kNow + base::Days(5) + base::Hours(8));
  policy_.RecordRecentUsageMetrics(data);
  histograms_.ExpectBucketCount("UserEducation.Session.ShortTermCount", 3, 1);
  histograms_.ExpectBucketCount("UserEducation.Session.LongTermCount", 6, 2);
}

// When there's only one threshold, both histograms record the same value.
TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_OneThreshold) {
  const RecentSessionPolicyImpl::UsageThresholds thresholds = {
      {3, base::Days(7)}};
  policy_.set_thresholds_for_testing(thresholds);

  auto data =
      CreateSessionData({base::Days(1), base::Days(2), base::Days(3),
                         base::Days(10), base::Days(20), base::Days(30)},
                        base::Days(60));
  policy_.RecordRecentUsageMetrics(data);
  histograms_.ExpectBucketCount("UserEducation.Session.ShortTermCount", 4, 1);
  histograms_.ExpectBucketCount("UserEducation.Session.LongTermCount", 4, 1);
}

// When there are many thresholds, only the first and last are used.
TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_ManyThresholds) {
  const RecentSessionPolicyImpl::UsageThresholds thresholds = {
      {3, base::Days(7)},
      {4, base::Days(11)},
      {5, base::Days(21)},
      {6, base::Days(60)}};
  policy_.set_thresholds_for_testing(thresholds);

  auto data =
      CreateSessionData({base::Days(1), base::Days(2), base::Days(3),
                         base::Days(10), base::Days(20), base::Days(30)},
                        base::Days(60));
  policy_.RecordRecentUsageMetrics(data);
  histograms_.ExpectBucketCount("UserEducation.Session.ShortTermCount", 4, 1);
  histograms_.ExpectBucketCount("UserEducation.Session.LongTermCount", 7, 1);
}

TEST_F(RecentSessionPolicyTest,
       RecordRecentUsageMetrics_JustEnabledDoesNotRecord) {
  const RecentSessionPolicyImpl::UsageThresholds thresholds = {
      {3, base::Days(7)}, {5, base::Days(21)}};
  policy_.set_thresholds_for_testing(thresholds);

  auto data =
      CreateSessionData({base::Days(1), base::Days(2), base::Days(3),
                         base::Days(10), base::Days(20), base::Days(30)},
                        base::Days(5));
  policy_.RecordRecentUsageMetrics(data);
  histograms_.ExpectBucketCount("UserEducation.Session.ShortTermCount", 4, 0);
  histograms_.ExpectBucketCount("UserEducation.Session.LongTermCount", 6, 0);
}

TEST_F(RecentSessionPolicyTest,
       RecordRecentUsageMetrics_EnabledDatePreventsLongTermCount) {
  const RecentSessionPolicyImpl::UsageThresholds thresholds = {
      {3, base::Days(7)}, {5, base::Days(21)}};
  policy_.set_thresholds_for_testing(thresholds);

  auto data =
      CreateSessionData({base::Days(1), base::Days(2), base::Days(3),
                         base::Days(10), base::Days(20), base::Days(30)},
                        base::Days(10));
  policy_.RecordRecentUsageMetrics(data);
  histograms_.ExpectBucketCount("UserEducation.Session.ShortTermCount", 4, 1);
  histograms_.ExpectBucketCount("UserEducation.Session.LongTermCount", 6, 0);
}

TEST_F(RecentSessionPolicyTest, ShouldEnableLowUsagePromoMode) {
  const RecentSessionPolicyImpl::UsageThresholds thresholds = {
      {3, base::Days(7)}, {5, base::Days(21)}};
  constexpr auto kSinceEnabled = base::Days(60);
  policy_.set_thresholds_for_testing(thresholds);

  // Only one session.
  auto data = CreateSessionData({}, kSinceEnabled);
  EXPECT_TRUE(policy_.ShouldEnableLowUsagePromoMode(data));

  // Not enough in any threshold.
  data = CreateSessionData({base::Days(2), base::Days(30)}, kSinceEnabled);
  EXPECT_TRUE(policy_.ShouldEnableLowUsagePromoMode(data));

  // First threshold hit.
  data = CreateSessionData({base::Days(1), base::Days(2), base::Days(30)},
                           kSinceEnabled);
  EXPECT_FALSE(policy_.ShouldEnableLowUsagePromoMode(data));

  // Second threshold hit.
  data = CreateSessionData({base::Days(1), base::Days(11), base::Days(12),
                            base::Days(13), base::Days(30)},
                           kSinceEnabled);
  EXPECT_FALSE(policy_.ShouldEnableLowUsagePromoMode(data));

  // First and second thresholds hit.
  data = CreateSessionData({base::Days(1), base::Days(2), base::Days(3),
                            base::Days(13), base::Days(30)},
                           kSinceEnabled);
  EXPECT_FALSE(policy_.ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyTest,
       ShouldEnableLowUsagePromoMode_OffWhenNotEnabledForLongEnough) {
  const RecentSessionPolicyImpl::UsageThresholds thresholds = {
      {3, base::Days(7)}, {5, base::Days(21)}};
  policy_.set_thresholds_for_testing(thresholds);

  // Only one session, but not enabled long enough for the long threshold.
  auto data = CreateSessionData({}, base::Days(19));
  EXPECT_FALSE(policy_.ShouldEnableLowUsagePromoMode(data));

  // Not enough in any threshold, not enabled long enough for any threshold.
  data = CreateSessionData({base::Days(2), base::Days(30)}, base::Days(5));
  EXPECT_FALSE(policy_.ShouldEnableLowUsagePromoMode(data));

  // Not enough in any threshold, enabled at the longer threshold.
  data = CreateSessionData({base::Days(2), base::Days(10), base::Days(30)},
                           base::Days(21));
  EXPECT_TRUE(policy_.ShouldEnableLowUsagePromoMode(data));
}
