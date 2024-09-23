// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/recent_session_policy.h"

#include <sstream>
#include <vector>

#include "base/containers/map_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Arbitrary date not too far from the current date.
// Use a time that will not be exactly midnight in any locale.
constexpr base::Time kNow = base::Time::FromDeltaSinceWindowsEpoch(
    base::Days(150000) + base::Minutes(23));

// Calculates a set of start times in terms of time before kNow.
// The most recent time will always be kNow; the other times are
// added after and should be in increasing order of time interval.
RecentSessionData CreateSessionData(
    const std::initializer_list<base::TimeDelta>& before_now,
    const std::optional<base::TimeDelta>& since_enabled = base::Days(30)) {
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

using Constraint = RecentSessionPolicyImpl::Constraint;
using ConstraintInfo = RecentSessionPolicyImpl::ConstraintInfo;
using ConstraintInfos = RecentSessionPolicyImpl::ConstraintInfos;

constexpr char kTestHistogramName[] = "Test.Histogram";
constexpr char kTestHistogramName2[] = "Test.Histogram2";

class MockConstraint : public Constraint {
 public:
  MOCK_METHOD(std::optional<int>,
              GetCount,
              (const RecentSessionData&),
              (const, override));
};

using StrictMockConstraint = testing::StrictMock<MockConstraint>;
using StrictMockConstraintPtr = raw_ptr<StrictMockConstraint>;

// Creates a constraint info with a StrictMockConstraint, which is output via
// `constraint`. Callers will need to set any expectations on the mock.
ConstraintInfo CreateMockConstraintInfo(StrictMockConstraintPtr& constraint,
                                        std::string histogram_name,
                                        std::optional<int> histogram_max,
                                        std::optional<int> low_usage) {
  auto constraint_ptr = std::make_unique<StrictMockConstraint>();
  constraint = constraint_ptr.get();
  return ConstraintInfo(std::move(constraint_ptr), std::move(histogram_name),
                        histogram_max, low_usage);
}

}  // namespace

class RecentSessionPolicyTest : public testing::Test {
 public:
  RecentSessionPolicyTest() = default;
  ~RecentSessionPolicyTest() override = default;

  void SetUp() override {
    // Default setup is to initialize right away, but this can be skipped in
    // other tests by overriding and replacing this behavior.
    Init();
  }

  void TearDown() override { policy_.reset(); }

  void Init(const base::FieldTrialParams& params = {}) {
    feature_list_.InitAndEnableFeatureWithParameters(
        kAllowRecentSessionTracking, params);
    policy_ = std::make_unique<RecentSessionPolicyImpl>();
  }

  void EnsureBucketCounts(const std::string& name,
                          std::map<int, int> counts) const {
    const auto buckets = histograms_.GetAllSamples(name);
    for (const auto& bucket : buckets) {
      if (bucket.count == 0) {
        continue;
      }
      int* const expected = base::FindOrNull(counts, bucket.min);
      if (!expected) {
        ADD_FAILURE() << "Got unexpected bucket in " << name << ": "
                      << bucket.min << " = " << bucket.count;
        continue;
      }
      EXPECT_EQ(*expected, bucket.count) << "Got bucket with wrong count in "
                                         << name << " for entry " << bucket.min;
      counts.erase(bucket.min);
    }

    for (const auto& count : counts) {
      if (count.second != 0) {
        ADD_FAILURE() << "Did not get expected entry in " << name << ": "
                      << count.first;
      }
    }
  }

 protected:
  base::HistogramTester histograms_;
  std::unique_ptr<RecentSessionPolicyImpl> policy_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RecentSessionPolicyTest,
       ShouldEnableLowUsagePromoMode_SingleConstraint) {
  RecentSessionData data;
  ConstraintInfos constraints;
  StrictMockConstraintPtr mock_constraint;
  constraints.emplace_back(
      CreateMockConstraintInfo(mock_constraint, "", std::nullopt, 2));
  policy_->set_constraints_for_testing(std::move(constraints));

  // Returns below the threshold.
  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Returns equal to the threshold.
  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(2));
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Returns above the threshold.
  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(3));
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyTest,
       ShouldEnableLowUsagePromoMode_MultipleConstraints) {
  RecentSessionData data;
  ConstraintInfos constraints;
  StrictMockConstraintPtr mock_constraint1;
  StrictMockConstraintPtr mock_constraint2;
  constraints.emplace_back(
      CreateMockConstraintInfo(mock_constraint1, "", std::nullopt, 2));
  constraints.emplace_back(
      CreateMockConstraintInfo(mock_constraint2, "", std::nullopt, 2));
  policy_->set_constraints_for_testing(std::move(constraints));

  // Both constraints below the threshold.
  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  EXPECT_CALL(*mock_constraint2, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // One constraint above the threshold.
  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(3));
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));

  // The other constraint above the threshold.
  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  EXPECT_CALL(*mock_constraint2, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(3));
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Both constraints above the threshold.
  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(3));
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyTest,
       ShouldEnableLowUsagePromoMode_HistogramOnlyConstraint) {
  RecentSessionData data;
  ConstraintInfos constraints;
  StrictMockConstraintPtr mock_constraint1;
  StrictMockConstraintPtr mock_constraint2;
  constraints.emplace_back(
      CreateMockConstraintInfo(mock_constraint1, "", std::nullopt, 2));
  constraints.emplace_back(CreateMockConstraintInfo(
      mock_constraint2, kTestHistogramName, 7, std::nullopt));
  policy_->set_constraints_for_testing(std::move(constraints));

  // Active constraint below the threshold.
  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Active constraint above the threshold.
  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(3));
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_SingleHistogram) {
  RecentSessionData data;
  ConstraintInfos constraints;
  StrictMockConstraintPtr mock_constraint;
  constraints.emplace_back(CreateMockConstraintInfo(
      mock_constraint, kTestHistogramName, 4, std::nullopt));
  policy_->set_constraints_for_testing(std::move(constraints));

  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  policy_->RecordRecentUsageMetrics(data);

  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(std::nullopt));
  policy_->RecordRecentUsageMetrics(data);

  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(2));
  policy_->RecordRecentUsageMetrics(data);

  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(2));
  policy_->RecordRecentUsageMetrics(data);

  EXPECT_CALL(*mock_constraint, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(7));
  policy_->RecordRecentUsageMetrics(data);

  histograms_.ExpectTotalCount(kTestHistogramName, 4);
  histograms_.ExpectBucketCount(kTestHistogramName, 1, 1);
  histograms_.ExpectBucketCount(kTestHistogramName, 2, 2);
  histograms_.ExpectBucketCount(kTestHistogramName, 4, 1);
}

TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_TwoHistograms) {
  RecentSessionData data;
  ConstraintInfos constraints;
  StrictMockConstraintPtr mock_constraint1;
  StrictMockConstraintPtr mock_constraint2;
  constraints.emplace_back(CreateMockConstraintInfo(
      mock_constraint1, kTestHistogramName, 4, std::nullopt));
  constraints.emplace_back(CreateMockConstraintInfo(
      mock_constraint2, kTestHistogramName2, 4, std::nullopt));
  policy_->set_constraints_for_testing(std::move(constraints));

  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  EXPECT_CALL(*mock_constraint2, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(std::nullopt));
  policy_->RecordRecentUsageMetrics(data);

  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(1));
  EXPECT_CALL(*mock_constraint2, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(2));
  policy_->RecordRecentUsageMetrics(data);

  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(2));
  EXPECT_CALL(*mock_constraint2, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(3));
  policy_->RecordRecentUsageMetrics(data);

  EXPECT_CALL(*mock_constraint1, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(4));
  EXPECT_CALL(*mock_constraint2, GetCount(testing::Ref(data)))
      .WillOnce(testing::Return(7));
  policy_->RecordRecentUsageMetrics(data);

  histograms_.ExpectTotalCount(kTestHistogramName, 4);
  histograms_.ExpectBucketCount(kTestHistogramName, 1, 2);
  histograms_.ExpectBucketCount(kTestHistogramName, 2, 1);
  histograms_.ExpectBucketCount(kTestHistogramName, 4, 1);

  histograms_.ExpectTotalCount(kTestHistogramName2, 3);
  histograms_.ExpectBucketCount(kTestHistogramName2, 2, 1);
  histograms_.ExpectBucketCount(kTestHistogramName2, 3, 1);
  histograms_.ExpectBucketCount(kTestHistogramName2, 4, 1);
}

TEST_F(RecentSessionPolicyTest, SessionCountConstraint) {
  RecentSessionPolicyImpl::SessionCountConstraint constraint(7);

  // Too short a time to render a count:
  EXPECT_EQ(std::nullopt, constraint.GetCount(CreateSessionData(
                              {base::Days(3)}, base::Days(5))));
  EXPECT_EQ(std::nullopt,
            constraint.GetCount(CreateSessionData(
                {base::Days(3)}, base::Days(7) - base::Seconds(1))));

  // Just long enough to render a count:
  EXPECT_EQ(2, constraint.GetCount(
                   CreateSessionData({base::Days(3)}, base::Days(7))));

  // Time outside window - only current session is counted:
  EXPECT_EQ(1, constraint.GetCount(CreateSessionData(
                   {base::Days(7) + base::Seconds(1)}, base::Days(7))));

  // Many times:
  EXPECT_EQ(4,
            constraint.GetCount(CreateSessionData(
                {base::Days(10), base::Days(6), base::Days(4), base::Days(2)},
                base::Days(7))));
}

TEST_F(RecentSessionPolicyTest, ActiveDaysConstraint) {
  RecentSessionPolicyImpl::ActiveDaysConstraint constraint(7);
  // Too short a time to render a count:
  EXPECT_EQ(std::nullopt, constraint.GetCount(CreateSessionData(
                              {base::Days(3)}, base::Days(5))));

  // Since days are counted back from the following midnight, exactly 7 days is
  // enough to enable counting.
  EXPECT_EQ(2, constraint.GetCount(
                   CreateSessionData({base::Days(3)}, base::Days(7))));

  // Multiple active days with more than one session on a day.
  EXPECT_EQ(3, constraint.GetCount(CreateSessionData(
                   {// In same day as most recent session.
                    base::Minutes(5),
                    // In previous day.
                    base::Days(1), base::Days(1) + base::Minutes(5),
                    // Several days ago.
                    base::Days(3)},
                   base::Days(10))));

  // Multiple active days with more than one session on a day, and sessions
  // outside the period.
  EXPECT_EQ(4, constraint.GetCount(CreateSessionData(
                   {base::Days(1), base::Days(3), base::Days(6), base::Days(8),
                    base::Days(12)},
                   base::Days(15))));
}

TEST_F(RecentSessionPolicyTest, ActiveWeeksConstraint) {
  RecentSessionPolicyImpl::ActiveWeeksConstraint constraint(4, 1);
  // Too short a time to render a count:
  EXPECT_EQ(std::nullopt, constraint.GetCount(CreateSessionData(
                              {base::Days(3)}, base::Days(27))));

  // Since days are counted back from the following midnight, exactly 28 days is
  // enough to enable counting.
  EXPECT_EQ(2, constraint.GetCount(
                   CreateSessionData({base::Days(10)}, base::Days(28))));

  // Multiple active weeks with more than one session in a week.
  EXPECT_EQ(3, constraint.GetCount(CreateSessionData(
                   {// In same week as most recent session.
                    base::Days(2), base::Days(4),
                    // Exactly seven days will shunt into a different week,
                    // because of counting from next midnight.
                    base::Days(7), base::Days(9),
                    // Three weeks ago.
                    base::Days(25)})));

  // Multiple active weeks with more than one session in a week, and sessions
  // outside the period.
  EXPECT_EQ(2, constraint.GetCount(CreateSessionData(
                   {base::Days(1), base::Days(3), base::Days(6), base::Days(8),
                    base::Days(12), base::Days(29)})));
}

TEST_F(RecentSessionPolicyTest, ActiveWeeksConstraintWithThreshold) {
  RecentSessionPolicyImpl::ActiveWeeksConstraint constraint(4, 2);
  // Too short a time to render a count:
  EXPECT_EQ(std::nullopt, constraint.GetCount(CreateSessionData(
                              {base::Days(3)}, base::Days(27))));

  // Since days are counted back from the following midnight, exactly 28 days is
  // enough to enable counting.
  EXPECT_EQ(0, constraint.GetCount(
                   CreateSessionData({base::Days(10)}, base::Days(28))));

  // Multiple active weeks with more than one session in a week.
  EXPECT_EQ(2, constraint.GetCount(CreateSessionData(
                   {// In same week as most recent session.
                    base::Days(2), base::Days(4),
                    // Exactly seven days will shunt into a different week,
                    // because of counting from next midnight.
                    base::Days(7), base::Days(9),
                    // Three weeks ago.
                    base::Days(25)})));

  // Multiple active weeks with more than one session in a week, and sessions
  // outside the period.
  EXPECT_EQ(3, constraint.GetCount(CreateSessionData(
                   {base::Days(1), base::Days(3), base::Days(6), base::Days(8),
                    base::Days(12), base::Days(19), base::Days(20),
                    base::Days(29)})));
}

TEST_F(RecentSessionPolicyTest,
       ShouldEnableLowUsagePromoMode_OffWhenNotEnabledForLongEnough) {
  // Only one session, but not enabled long enough for the long threshold.
  auto data = CreateSessionData({}, base::Days(19));
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Not enough in any threshold, not enabled long enough for any threshold.
  data = CreateSessionData({base::Days(2), base::Days(15)}, base::Days(25));
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Not enough in any threshold, enabled at the longer threshold.
  data = CreateSessionData({base::Days(10)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyTest,
       ShouldEnableLowUsagePromoMode_OffMoreThanTwoActiveDays) {
  // Two days, same week.
  auto data = CreateSessionData({base::Days(1)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Two days, different weeks.
  data = CreateSessionData({base::Days(8)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Three days one week.
  data = CreateSessionData({base::Days(1), base::Days(3)});
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Three days multiple weeks.
  data = CreateSessionData({base::Days(2), base::Days(22)});
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_LessThanOneWeek) {
  policy_->RecordRecentUsageMetrics(
      CreateSessionData({base::Days(1), base::Days(2)}, base::Days(4)));
  EnsureBucketCounts("UserEducation.Session.ShortTermCount", {});
  EnsureBucketCounts("UserEducation.Session.LongTermCount", {});
  EnsureBucketCounts("UserEducation.Session.MonthlyActiveDays", {});
  EnsureBucketCounts("UserEducation.Session.RecentActiveDays", {});
  EnsureBucketCounts("UserEducation.Session.RecentActiveWeeks", {});
  EnsureBucketCounts("UserEducation.Session.RecentSuperActiveWeeks", {});
}

TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_MoreThanOneWeek) {
  policy_->RecordRecentUsageMetrics(
      CreateSessionData({base::Days(1), base::Days(1) + base::Minutes(5),
                         base::Days(2), base::Days(6)},
                        base::Days(8)));
  EnsureBucketCounts("UserEducation.Session.ShortTermCount", {{5, 1}});
  EnsureBucketCounts("UserEducation.Session.LongTermCount", {});
  EnsureBucketCounts("UserEducation.Session.MonthlyActiveDays", {});
  EnsureBucketCounts("UserEducation.Session.RecentActiveDays", {{4, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveWeeks", {});
  EnsureBucketCounts("UserEducation.Session.RecentSuperActiveWeeks", {});
}

TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_FullPeriod) {
  policy_->RecordRecentUsageMetrics(CreateSessionData(
      {base::Days(1), base::Days(1) + base::Minutes(5), base::Days(2),
       base::Days(6), base::Days(15), base::Days(20)}));
  EnsureBucketCounts("UserEducation.Session.ShortTermCount", {{5, 1}});
  EnsureBucketCounts("UserEducation.Session.LongTermCount", {{7, 1}});
  EnsureBucketCounts("UserEducation.Session.MonthlyActiveDays", {{6, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveDays", {{4, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveWeeks", {{2, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentSuperActiveWeeks", {{1, 1}});
}

TEST_F(RecentSessionPolicyTest,
       RecordRecentUsageMetrics_SuperActiveCountsDaysNotSessions) {
  policy_->RecordRecentUsageMetrics(CreateSessionData(
      {// Initial week with four active days (counting day zero) and no
       // additional sessions.
       base::Days(1), base::Days(2), base::Days(6),
       // Second week with three active days but four sessions.
       base::Days(15), base::Days(15) + base::Minutes(5),
       base::Days(15) + base::Minutes(10),
       base::Days(15) + base::Minutes(15)}));
  EnsureBucketCounts("UserEducation.Session.ShortTermCount", {{4, 1}});
  EnsureBucketCounts("UserEducation.Session.LongTermCount", {{8, 1}});
  EnsureBucketCounts("UserEducation.Session.MonthlyActiveDays", {{5, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveDays", {{4, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveWeeks", {{2, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentSuperActiveWeeks", {{1, 1}});
}

TEST_F(RecentSessionPolicyTest, RecordRecentUsageMetrics_DailyLimit) {
  auto data = CreateSessionData(
      {base::Days(1), base::Days(1) + base::Minutes(5), base::Days(2),
       base::Days(6), base::Days(15), base::Days(20)});
  policy_->RecordRecentUsageMetrics(data);
  EnsureBucketCounts("UserEducation.Session.ShortTermCount", {{5, 1}});
  EnsureBucketCounts("UserEducation.Session.LongTermCount", {{7, 1}});
  EnsureBucketCounts("UserEducation.Session.MonthlyActiveDays", {{6, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveDays", {{4, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveWeeks", {{2, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentSuperActiveWeeks", {{1, 1}});

  // Start another session almost right away, so it's in the same day.
  data.recent_session_start_times.insert(
      data.recent_session_start_times.begin(),
      data.recent_session_start_times.front() + base::Seconds(5));
  policy_->RecordRecentUsageMetrics(data);
  // Session-based metrics should still be recorded.
  EnsureBucketCounts("UserEducation.Session.ShortTermCount", {{5, 1}, {6, 1}});
  EnsureBucketCounts("UserEducation.Session.LongTermCount", {{7, 1}, {8, 1}});
  // Daily and weekly metrics, however, should not.
  EnsureBucketCounts("UserEducation.Session.MonthlyActiveDays", {{6, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveDays", {{4, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveWeeks", {{2, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentSuperActiveWeeks", {{1, 1}});

  // Start another session on the next calendar day.
  data.recent_session_start_times.insert(
      data.recent_session_start_times.begin(),
      data.recent_session_start_times.front() + base::Days(1));
  policy_->RecordRecentUsageMetrics(data);
  // All metrics should now be recorded. Some days will have shifted to the next
  // week.
  EnsureBucketCounts("UserEducation.Session.ShortTermCount", {{5, 1}, {6, 2}});
  EnsureBucketCounts("UserEducation.Session.LongTermCount",
                     {{7, 1}, {8, 1}, {9, 1}});
  EnsureBucketCounts("UserEducation.Session.MonthlyActiveDays",
                     {{6, 1}, {7, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveDays", {{4, 2}});
  EnsureBucketCounts("UserEducation.Session.RecentActiveWeeks",
                     {{2, 1}, {4, 1}});
  EnsureBucketCounts("UserEducation.Session.RecentSuperActiveWeeks", {{1, 2}});
}

class RecentSessionPolicyFinchTest : public RecentSessionPolicyTest {
 public:
  RecentSessionPolicyFinchTest() = default;
  ~RecentSessionPolicyFinchTest() override = default;

  // Don't initialize during setup.
  void SetUp() override {}
};

TEST_F(RecentSessionPolicyFinchTest, ChangeExistingThresholds) {
  Init({{"max_active_weeks", "1"}, {"max_active_days", "2"}});

  // Two days, one week.
  auto data = CreateSessionData({base::Days(2)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Two days, one week, but more entries.
  data = CreateSessionData({base::Days(2), base::Days(2) + base::Minutes(5)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Three days, one week.
  data = CreateSessionData({base::Days(2), base::Days(4)});
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Two days, two weeks.
  data = CreateSessionData({base::Days(2), base::Days(8)});
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyFinchTest, SwitchToNewThresholds) {
  Init({{"max_active_weeks", "0"},
        {"max_active_days", "0"},
        {"max_monthly_active_days", "0"},
        {"max_weekly_sessions", "2"},
        {"max_monthly_sessions", "5"}});

  // Two sessions.
  auto data = CreateSessionData({base::Days(2)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Three sessions, two days.
  data = CreateSessionData({base::Days(2), base::Days(2) + base::Minutes(5)});
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Three sessions spread over three active weeks.
  data = CreateSessionData({base::Days(8), base::Days(16)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Six sessions in two active weeks.
  data = CreateSessionData({base::Days(2), base::Days(8), base::Days(9),
                            base::Days(10), base::Days(11)});
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));
}

TEST_F(RecentSessionPolicyFinchTest, EnableSuperActiveThreshold) {
  Init({{"max_active_days", "0"},
        {"max_active_weeks", "0"},
        {"max_monthly_active_days", "0"},
        {"super_active_days", "3"},
        {"max_super_active_weeks", "1"}});

  // Two sessions.
  auto data = CreateSessionData({base::Days(2)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Three sessions, two weeks.
  data = CreateSessionData({base::Days(2), base::Days(12)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Three weeks, one super active week.
  data = CreateSessionData({base::Days(8), base::Days(9), base::Days(10),
                            base::Days(11), base::Days(16)});
  EXPECT_TRUE(policy_->ShouldEnableLowUsagePromoMode(data));

  // Two super active weeks by minimum definition.
  data = CreateSessionData({base::Days(8), base::Days(9), base::Days(10),
                            base::Days(15), base::Days(16), base::Days(17)});
  EXPECT_FALSE(policy_->ShouldEnableLowUsagePromoMode(data));
}
