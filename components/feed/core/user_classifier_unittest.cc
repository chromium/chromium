// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/user_classifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoubleNear;
using testing::Eq;
using testing::Gt;
using testing::Lt;
using testing::SizeIs;

namespace feed {

namespace {

char kNowString[] = "2017-03-01 10:45";

class FeedUserClassifierTest : public testing::Test {
 public:
  FeedUserClassifierTest() {
    UserClassifier::RegisterProfilePrefs(test_prefs_.registry());
  }

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

  DISALLOW_COPY_AND_ASSIGN(FeedUserClassifierTest);
};

TEST_F(FeedUserClassifierTest, ShouldBeActiveSuggestionsViewerInitially) {
  UserClassifier* user_classifier = CreateUserClassifier();
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kActiveSuggestionsViewer));
}

TEST_F(FeedUserClassifierTest,
       ShouldBecomeActiveSuggestionsConsumerByClickingOften) {
  UserClassifier* user_classifier = CreateUserClassifier();

  // After one click still only an active user.
  user_classifier->OnEvent(UserClassifier::Event::kSuggestionsUsed);
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kActiveSuggestionsViewer));

  // After a few more clicks, become an active consumer.
  for (int i = 0; i < 5; i++) {
    test_clock()->Advance(base::TimeDelta::FromHours(1));
    user_classifier->OnEvent(UserClassifier::Event::kSuggestionsUsed);
  }
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kActiveSuggestionsConsumer));
}

TEST_F(FeedUserClassifierTest,
       ShouldBecomeActiveSuggestionsConsumerByClickingOftenWithDecreasedParam) {
  // Increase the param to one half.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"user_classifier_active_consumer_clicks_at_least_once_per_hours",
        "36"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // After two clicks still only an active user.
  user_classifier->OnEvent(UserClassifier::Event::kSuggestionsUsed);
  test_clock()->Advance(base::TimeDelta::FromHours(1));
  user_classifier->OnEvent(UserClassifier::Event::kSuggestionsUsed);
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kActiveSuggestionsViewer));

  // One more click to become an active consumer.
  test_clock()->Advance(base::TimeDelta::FromHours(1));
  user_classifier->OnEvent(UserClassifier::Event::kSuggestionsUsed);
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kActiveSuggestionsConsumer));
}

TEST_F(FeedUserClassifierTest,
       ShouldBecomeRareSuggestionsViewerUserByNoActivity) {
  UserClassifier* user_classifier = CreateUserClassifier();

  // After two days of waiting still an active user.
  test_clock()->Advance(base::TimeDelta::FromDays(2));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kActiveSuggestionsViewer));

  // Two more days to become a rare user.
  test_clock()->Advance(base::TimeDelta::FromDays(2));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kRareSuggestionsViewer));
}

TEST_F(FeedUserClassifierTest,
       ShouldBecomeRareSuggestionsViewerByNoActivityWithDecreasedParam) {
  // Decrease the param to one half.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"user_classifier_rare_user_views_at_most_once_per_hours", "48"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // After one days of waiting still an active user.
  test_clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kActiveSuggestionsViewer));

  // One more day to become a rare user.
  test_clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_THAT(user_classifier->GetUserClass(),
              Eq(UserClassifier::UserClass::kRareSuggestionsViewer));
}

class FeedUserClassifierEventTest
    : public FeedUserClassifierTest,
      public ::testing::WithParamInterface<
          std::pair<UserClassifier::Event, std::string>> {
 public:
  FeedUserClassifierEventTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedUserClassifierEventTest);
};

TEST_P(FeedUserClassifierEventTest, ShouldDecreaseEstimateAfterEvent) {
  UserClassifier::Event event = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event. does not decrease the estimate.
  user_classifier->OnEvent(event);

  for (int i = 0; i < 10; i++) {
    test_clock()->Advance(base::TimeDelta::FromHours(1));
    double old_rate = user_classifier->GetEstimatedAvgTime(event);
    user_classifier->OnEvent(event);
    EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event), Lt(old_rate));
  }
}

TEST_P(FeedUserClassifierEventTest, ShouldReportToUmaOnEvent) {
  UserClassifier::Event event = GetParam().first;
  const std::string& histogram_name = GetParam().second;
  base::HistogramTester histogram_tester;
  UserClassifier* user_classifier = CreateUserClassifier();

  user_classifier->OnEvent(event);
  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name), SizeIs(1));
}

TEST_P(FeedUserClassifierEventTest, ShouldConvergeTowardsPattern) {
  UserClassifier::Event event = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // Have the pattern of an event every five hours and start changing it towards
  // an event every 10 hours.
  for (int i = 0; i < 100; i++) {
    test_clock()->Advance(base::TimeDelta::FromHours(5));
    user_classifier->OnEvent(event);
  }
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event),
              DoubleNear(5.0, 0.1));
  for (int i = 0; i < 3; i++) {
    test_clock()->Advance(base::TimeDelta::FromHours(10));
    user_classifier->OnEvent(event);
  }
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event), Gt(5.5));
  for (int i = 0; i < 100; i++) {
    test_clock()->Advance(base::TimeDelta::FromHours(10));
    user_classifier->OnEvent(event);
  }
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event),
              DoubleNear(10.0, 0.1));
}

TEST_P(FeedUserClassifierEventTest, ShouldIgnoreSubsequentEventsForHalfAnHour) {
  UserClassifier::Event event = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event.
  user_classifier->OnEvent(event);
  // Subsequent events get ignored for the next 30 minutes.
  for (int i = 0; i < 5; i++) {
    test_clock()->Advance(base::TimeDelta::FromMinutes(5));
    double old_rate = user_classifier->GetEstimatedAvgTime(event);
    user_classifier->OnEvent(event);
    EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event), Eq(old_rate));
  }
  // An event 30 minutes after the initial event is finally not ignored.
  test_clock()->Advance(base::TimeDelta::FromMinutes(5));
  double old_rate = user_classifier->GetEstimatedAvgTime(event);
  user_classifier->OnEvent(event);
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event), Lt(old_rate));
}

TEST_P(FeedUserClassifierEventTest,
       ShouldIgnoreSubsequentEventsWithIncreasedLimit) {
  UserClassifier::Event event = GetParam().first;
  // Increase the min_hours to 1.0, i.e. 60 minutes.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions, {{"user_classifier_min_hours", "1.0"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event.
  user_classifier->OnEvent(event);
  // Subsequent events get ignored for the next 60 minutes.
  for (int i = 0; i < 11; i++) {
    test_clock()->Advance(base::TimeDelta::FromMinutes(5));
    double old_rate = user_classifier->GetEstimatedAvgTime(event);
    user_classifier->OnEvent(event);
    EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event), Eq(old_rate));
  }
  // An event 60 minutes after the initial event is finally not ignored.
  test_clock()->Advance(base::TimeDelta::FromMinutes(5));
  double old_rate = user_classifier->GetEstimatedAvgTime(event);
  user_classifier->OnEvent(event);
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event), Lt(old_rate));
}

TEST_P(FeedUserClassifierEventTest, ShouldCapDelayBetweenEvents) {
  UserClassifier::Event event = GetParam().first;
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event.
  user_classifier->OnEvent(event);
  // Wait for an insane amount of time
  test_clock()->Advance(base::TimeDelta::FromDays(365));
  user_classifier->OnEvent(event);
  double rate_after_a_year = user_classifier->GetEstimatedAvgTime(event);

  // Now repeat the same with s/one year/one week.
  user_classifier->ClearClassificationForDebugging();
  user_classifier->OnEvent(event);
  test_clock()->Advance(base::TimeDelta::FromDays(7));
  user_classifier->OnEvent(event);

  // The results should be the same.
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event),
              Eq(rate_after_a_year));
}

TEST_P(FeedUserClassifierEventTest,
       ShouldCapDelayBetweenEventsWithDecreasedLimit) {
  UserClassifier::Event event = GetParam().first;
  // Decrease the max_hours to 72, i.e. 3 days.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions, {{"user_classifier_max_hours", "72"}});
  UserClassifier* user_classifier = CreateUserClassifier();

  // The initial event.
  user_classifier->OnEvent(event);
  // Wait for an insane amount of time
  test_clock()->Advance(base::TimeDelta::FromDays(365));
  user_classifier->OnEvent(event);
  double rate_after_a_year = user_classifier->GetEstimatedAvgTime(event);

  // Now repeat the same with s/one year/two days.
  user_classifier->ClearClassificationForDebugging();
  user_classifier->OnEvent(event);
  test_clock()->Advance(base::TimeDelta::FromDays(3));
  user_classifier->OnEvent(event);

  // The results should be the same.
  EXPECT_THAT(user_classifier->GetEstimatedAvgTime(event),
              Eq(rate_after_a_year));
}

INSTANTIATE_TEST_SUITE_P(
    ,  // An empty prefix for the parametrized tests names (no need to
       // distinguish the only instance we make here).
    FeedUserClassifierEventTest,
    testing::Values(
        std::make_pair(UserClassifier::Event::kSuggestionsViewed,
                       "NewTabPage.UserClassifier.AverageHoursToOpenNTP"),
        std::make_pair(
            UserClassifier::Event::kSuggestionsUsed,
            "NewTabPage.UserClassifier.AverageHoursToUseSuggestions")));
}  // namespace

}  // namespace feed
