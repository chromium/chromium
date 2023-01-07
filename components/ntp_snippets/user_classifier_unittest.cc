// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/user_classifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoubleNear;
using testing::Eq;
using testing::Gt;
using testing::Lt;
using testing::SizeIs;

namespace ntp_snippets {
namespace {

char kNowString[] = "2017-03-01 10:45";

class UserClassifierTest : public testing::Test {
 public:
  UserClassifierTest() {
    UserClassifier::RegisterProfilePrefs(test_prefs_.registry());
  }
  UserClassifierTest(const UserClassifierTest&) = delete;
  UserClassifierTest& operator=(const UserClassifierTest&) = delete;

  UserClassifier* CreateUserClassifier() {
    base::Time now;
    CHECK(base::Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);

    user_classifier_ =
        std::make_unique<UserClassifier>(&test_prefs_, &test_clock_);
    return user_classifier_.get();
  }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

 private:
  TestingPrefServiceSimple test_prefs_;
  std::unique_ptr<UserClassifier> user_classifier_;
  base::SimpleTestClock test_clock_;
};

TEST_F(UserClassifierTest, ShouldBeActiveNtpUserInitially) {
  UserClassifier* user_classifier = CreateUserClassifier();
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::ACTIVE_NTP_USER));
}

TEST_F(UserClassifierTest,
       ShouldBecomeActiveSuggestionsConsumerByClickingOften) {
  UserClassifier* user_classifier = CreateUserClassifier();

  // After one click still only an active user.
  user_classifier->OnEvent(UserClassifier::Metric::SUGGESTIONS_USED);
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::ACTIVE_NTP_USER));

  // After a few more clicks, become an active consumer.
  for (int i = 0; i < 5; i++) {
    test_clock()->Advance(base::Hours(1));
    user_classifier->OnEvent(UserClassifier::Metric::SUGGESTIONS_USED);
  }
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER));
}

TEST_F(UserClassifierTest,
       ShouldBecomeActiveSuggestionsConsumerByClickingOftenWithDecreasedParam) {
  // Increase the param to one half.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kArticleSuggestionsFeature,
      {{"user_classifier_active_consumer_clicks_at_least_once_per_hours",
        "36"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // After two clicks still only an active user.
  user_classifier->OnEvent(UserClassifier::Metric::SUGGESTIONS_USED);
  test_clock()->Advance(base::Hours(1));
  user_classifier->OnEvent(UserClassifier::Metric::SUGGESTIONS_USED);
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::ACTIVE_NTP_USER));

  // One more click to become an active consumer.
  test_clock()->Advance(base::Hours(1));
  user_classifier->OnEvent(UserClassifier::Metric::SUGGESTIONS_USED);
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER));
}

TEST_F(UserClassifierTest, ShouldBecomeRareNtpUserByNoActivity) {
  UserClassifier* user_classifier = CreateUserClassifier();

  // After two days of waiting still an active user.
  test_clock()->Advance(base::Days(2));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::ACTIVE_NTP_USER));

  // Two more days to become a rare user.
  test_clock()->Advance(base::Days(2));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::RARE_NTP_USER));
}

TEST_F(UserClassifierTest,
       ShouldBecomeRareNtpUserByNoActivityWithDecreasedParam) {
  // Decrease the param to one half.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kArticleSuggestionsFeature,
      {{"user_classifier_rare_user_opens_ntp_at_most_once_per_hours", "48"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // After one days of waiting still an active user.
  test_clock()->Advance(base::Days(1));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::ACTIVE_NTP_USER));

  // One more day to become a rare user.
  test_clock()->Advance(base::Days(1));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::RARE_NTP_USER));
}

class UserClassifierMetricTest
    : public UserClassifierTest,
      public ::testing::WithParamInterface<
          std::pair<UserClassifier::Metric, std::string>> {
 public:
  UserClassifierMetricTest() = default;
  UserClassifierMetricTest(const UserClassifierMetricTest&) = delete;
  UserClassifierMetricTest& operator=(const UserClassifierMetricTest&) = delete;
};

TEST_P(UserClassifierMetricTest, ShouldDecreaseEstimateAfterEvent) {
  UserClassifier::Metric metric = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event does not decrease the estimate.
  user_classifier->OnEvent(metric);

  for (int i = 0; i < 10; i++) {
    test_clock()->Advance(base::Hours(1));
    double old_metric = user_classifier->GetEstimatedAvgTime(metric);
    user_classifier->OnEvent(metric);
    EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric), Lt(old_metric));
  }
}

TEST_P(UserClassifierMetricTest, ShouldReportToUmaOnEvent) {
  UserClassifier::Metric metric = GetParam().first;
  const std::string& histogram_name = GetParam().second;
  base::HistogramTester histogram_tester;
  UserClassifier* user_classifier = CreateUserClassifier();

  user_classifier->OnEvent(metric);
  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name), SizeIs(1));
}

TEST_P(UserClassifierMetricTest, ShouldConvergeTowardsPattern) {
  UserClassifier::Metric metric = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // Have the pattern of an event every five hours and start changing it towards
  // an event every 10 hours.
  for (int i = 0; i < 100; i++) {
    test_clock()->Advance(base::Hours(5));
    user_classifier->OnEvent(metric);
  }
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric),
              DoubleNear(5.0, 0.1));
  for (int i = 0; i < 3; i++) {
    test_clock()->Advance(base::Hours(10));
    user_classifier->OnEvent(metric);
  }
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric), Gt(5.5));
  for (int i = 0; i < 100; i++) {
    test_clock()->Advance(base::Hours(10));
    user_classifier->OnEvent(metric);
  }
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric),
              DoubleNear(10.0, 0.1));
}

TEST_P(UserClassifierMetricTest, ShouldIgnoreSubsequentEventsForHalfAnHour) {
  UserClassifier::Metric metric = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event
  user_classifier->OnEvent(metric);
  // Subsequent events get ignored for the next 30 minutes.
  for (int i = 0; i < 5; i++) {
    test_clock()->Advance(base::Minutes(5));
    double old_metric = user_classifier->GetEstimatedAvgTime(metric);
    user_classifier->OnEvent(metric);
    EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric), Eq(old_metric));
  }
  // An event 30 minutes after the initial event is finally not ignored.
  test_clock()->Advance(base::Minutes(5));
  double old_metric = user_classifier->GetEstimatedAvgTime(metric);
  user_classifier->OnEvent(metric);
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric), Lt(old_metric));
}

TEST_P(UserClassifierMetricTest,
       ShouldIgnoreSubsequentEventsWithIncreasedLimit) {
  UserClassifier::Metric metric = GetParam().first;
  // Increase the min_hours to 1.0, i.e. 60 minutes.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kArticleSuggestionsFeature, {{"user_classifier_min_hours", "1.0"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event
  user_classifier->OnEvent(metric);
  // Subsequent events get ignored for the next 60 minutes.
  for (int i = 0; i < 11; i++) {
    test_clock()->Advance(base::Minutes(5));
    double old_metric = user_classifier->GetEstimatedAvgTime(metric);
    user_classifier->OnEvent(metric);
    EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric), Eq(old_metric));
  }
  // An event 60 minutes after the initial event is finally not ignored.
  test_clock()->Advance(base::Minutes(5));
  double old_metric = user_classifier->GetEstimatedAvgTime(metric);
  user_classifier->OnEvent(metric);
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric), Lt(old_metric));
}

TEST_P(UserClassifierMetricTest, ShouldCapDelayBetweenEvents) {
  UserClassifier::Metric metric = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event
  user_classifier->OnEvent(metric);
  // Wait for an insane amount of time
  test_clock()->Advance(base::Days(365));
  user_classifier->OnEvent(metric);
  double metric_after_a_year = user_classifier->GetEstimatedAvgTime(metric);

  // Now repeat the same with s/one year/one week.
  user_classifier->ClearClassificationForDebugging();
  user_classifier->OnEvent(metric);
  test_clock()->Advance(base::Days(7));
  user_classifier->OnEvent(metric);

  // The results should be the same.
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric),
              Eq(metric_after_a_year));
}

TEST_P(UserClassifierMetricTest,
       ShouldCapDelayBetweenEventsWithDecreasedLimit) {
  UserClassifier::Metric metric = GetParam().first;
  // Decrease the max_hours to 72, i.e. 3 days.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kArticleSuggestionsFeature, {{"user_classifier_max_hours", "72"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event
  user_classifier->OnEvent(metric);
  // Wait for an insane amount of time
  test_clock()->Advance(base::Days(365));
  user_classifier->OnEvent(metric);
  double metric_after_a_year = user_classifier->GetEstimatedAvgTime(metric);

  // Now repeat the same with s/one year/two days.
  user_classifier->ClearClassificationForDebugging();
  user_classifier->OnEvent(metric);
  test_clock()->Advance(base::Days(3));
  user_classifier->OnEvent(metric);

  // The results should be the same.
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(metric),
              Eq(metric_after_a_year));
}

INSTANTIATE_TEST_SUITE_P(
    NTP,
    UserClassifierMetricTest,
    testing::Values(
        std::make_pair(UserClassifier::Metric::NTP_OPENED,
                       "NewTabPage.UserClassifier.AverageHoursToOpenNTP"),
        std::make_pair(
            UserClassifier::Metric::SUGGESTIONS_SHOWN,
            "NewTabPage.UserClassifier.AverageHoursToShowSuggestions"),
        std::make_pair(
            UserClassifier::Metric::SUGGESTIONS_USED,
            "NewTabPage.UserClassifier.AverageHoursToUseSuggestions")));

}  // namespace
}  // namespace ntp_snippets
