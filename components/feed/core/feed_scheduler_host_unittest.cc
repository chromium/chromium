// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/refresh_throttler.h"
#include "components/feed/core/time_serialization.h"
#include "components/feed/core/user_classifier.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace feed {

namespace {

// Fixed "now" to make tests more deterministic.
char kNowString[] = "2018-06-11 15:41";

}  // namespace

class ForceDeviceOffline {
 public:
  ForceDeviceOffline() {
    notifier_->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

 private:
  std::unique_ptr<net::test::MockNetworkChangeNotifier> notifier_ =
      net::test::MockNetworkChangeNotifier::Create();
};

class FeedSchedulerHostTest : public ::testing::Test {
 public:
  void FixedTimerCompletion() { fixed_timer_completion_count_++; }

 protected:
  FeedSchedulerHostTest() {
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    local_state()->registry()->RegisterBooleanPref(::prefs::kEulaAccepted,
                                                   true);
    profile_prefs()->registry()->RegisterBooleanPref(
        prefs::kArticlesListVisible, true);

    Time now;
    EXPECT_TRUE(Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);

    NewScheduler();
  }

  void InitializeScheduler(FeedSchedulerHost* scheduler) {
    scheduler->Initialize(
        base::BindRepeating(&FeedSchedulerHostTest::TriggerRefresh,
                            base::Unretained(this)),
        base::BindRepeating(&FeedSchedulerHostTest::ScheduleWakeUp,
                            base::Unretained(this)),
        base::BindRepeating(&FeedSchedulerHostTest::CancelWakeUp,
                            base::Unretained(this)));
  }

  // Recreates a new copy of the scheduler. This is useful if a test case needs
  // to change some global state like prefs or params before the scheduler's
  // constructor runs.
  void NewScheduler() {
    scheduler_ = std::make_unique<FeedSchedulerHost>(
        &profile_prefs_, &local_state_, &test_clock_);
    InitializeScheduler(scheduler());
  }

  // Note: Time will be advanced.
  void ClassifyAsRareNtpUser() {
    // By moving time forward from initial seed events, the user will be moved
    // into kRareNtpUser classification.
    test_clock()->Advance(TimeDelta::FromDays(7));

    ASSERT_EQ(UserClassifier::UserClass::kRareSuggestionsViewer,
              scheduler()->GetUserClassifierForDebugging()->GetUserClass());
  }

  // Note: Time will be advanced.
  void ClassifyAsActiveSuggestionsConsumer() {
    // Click on some articles to move the user into kActiveSuggestionsConsumer
    // classification. Separate by at least 30 minutes for different sessions.
    scheduler()->OnSuggestionConsumed();
    test_clock()->Advance(TimeDelta::FromMinutes(31));
    scheduler()->OnSuggestionConsumed();

    // Depending on which events occurred over which period of time in the test
    // before this function was called, it may not necessarily be sufficient to
    // push the user into the active consumer class.
    ASSERT_EQ(UserClassifier::UserClass::kActiveSuggestionsConsumer,
              scheduler()->GetUserClassifierForDebugging()->GetUserClass());
  }

  // Many test cases want to ask the scheduler multiple times in a row to see
  // which of the different triggers or under which conditions the scheduler
  // will request a refresh. However the scheduler updates internal state when
  // it decides a refresh must be made, most importantly, it sets
  // |tracking_oustanding_request_| to true. Any subsequent trigger would then
  // not start a refresh. To get around this, this method clears out
  // |tracking_oustanding_request_|.
  void ResetRefreshState(Time last_attempt) {
    // OnRequestError() has the side effect of setting kLastFetchAttemptTime to
    // the scheduler's clock's now. This typically is not helpful to most test
    // cases, so override it.
    scheduler()->OnRequestError(0);
    profile_prefs()->SetTime(prefs::kLastFetchAttemptTime, last_attempt);
  }

  bool PlatformSupportsEula() {
    return web_resource::EulaAcceptedNotifier::Create(local_state()) != nullptr;
  }

  // This helper method sets prefs::kLastFetchAttemptTime to the same value
  // that's about to be passed into ShouldSessionRequestData(). This is what the
  // scheduler will typically experience when refreshes are successful. Also
  // clears out |tracking_oustanding_request_| through OnRequestError().
  NativeRequestBehavior ShouldSessionRequestData(
      bool has_content,
      Time content_creation_date_time,
      bool has_outstanding_request) {
    ResetRefreshState(content_creation_date_time);
    return scheduler()->ShouldSessionRequestData(
        has_content, content_creation_date_time, has_outstanding_request);
  }

  TestingPrefServiceSimple* profile_prefs() { return &profile_prefs_; }
  TestingPrefServiceSimple* local_state() { return &local_state_; }
  base::SimpleTestClock* test_clock() { return &test_clock_; }
  FeedSchedulerHost* scheduler() { return scheduler_.get(); }
  int refresh_call_count() { return refresh_call_count_; }
  const std::vector<TimeDelta>& schedule_wake_up_times() {
    return schedule_wake_up_times_;
  }
  int cancel_wake_up_call_count() { return cancel_wake_up_call_count_; }
  int fixed_timer_completion_count() { return fixed_timer_completion_count_; }

 private:
  void TriggerRefresh() { refresh_call_count_++; }

  void ScheduleWakeUp(TimeDelta threshold_ms) {
    schedule_wake_up_times_.push_back(threshold_ms);
  }

  void CancelWakeUp() { cancel_wake_up_call_count_++; }

  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<FeedSchedulerHost> scheduler_;
  int refresh_call_count_ = 0;
  std::vector<TimeDelta> schedule_wake_up_times_;
  int cancel_wake_up_call_count_ = 0;
  int fixed_timer_completion_count_ = 0;
  base::WeakPtrFactory<FeedSchedulerHostTest> weak_factory_{this};
};

TEST_F(FeedSchedulerHostTest, GetTriggerThreshold) {
  // Make sure that there is no missing configuration in the Cartesian product
  // of states between TriggerType and UserClass.
  std::vector<FeedSchedulerHost::TriggerType> triggers = {
      FeedSchedulerHost::TriggerType::kNtpShown,
      FeedSchedulerHost::TriggerType::kForegrounded,
      FeedSchedulerHost::TriggerType::kFixedTimer};

  // Classification starts out as an active NTP user.
  for (FeedSchedulerHost::TriggerType trigger : triggers) {
    EXPECT_FALSE(scheduler()->GetTriggerThreshold(trigger).is_zero());
  }

  ClassifyAsActiveSuggestionsConsumer();
  for (FeedSchedulerHost::TriggerType trigger : triggers) {
    EXPECT_FALSE(scheduler()->GetTriggerThreshold(trigger).is_zero());
  }

  ClassifyAsRareNtpUser();
  for (FeedSchedulerHost::TriggerType trigger : triggers) {
    EXPECT_FALSE(scheduler()->GetTriggerThreshold(trigger).is_zero());
  }
}

TEST_F(FeedSchedulerHostTest, ShouldSessionRequestDataSimple) {
  // For an kActiveNtpUser, refreshes on NTP_OPEN should be triggered after 4
  // hours, and staleness should be at 24 hours. Each case tests a range of
  // values.
  Time no_refresh_large = test_clock()->Now() - TimeDelta::FromHours(3);
  Time refresh_only_small = test_clock()->Now() - TimeDelta::FromHours(5);
  Time refresh_only_large = test_clock()->Now() - TimeDelta::FromHours(23);
  Time stale_small = test_clock()->Now() - TimeDelta::FromHours(25);

  EXPECT_EQ(kRequestWithWait,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(kRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ refresh_only_small,
                /*has_outstanding_request*/ false));
  EXPECT_EQ(kRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ refresh_only_large,
                /*has_outstanding_request*/ false));

  EXPECT_EQ(kRequestWithTimeout, ShouldSessionRequestData(
                                     /*has_content*/ true,
                                     /*content_creation_date_time*/ stale_small,
                                     /*has_outstanding_request*/ false));
  EXPECT_EQ(kRequestWithTimeout,
            ShouldSessionRequestData(
                /*has_content*/ true, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));

  // |content_creation_date_time| should be ignored when |has_content| is false.
  EXPECT_EQ(kNoRequestWithWait,
            ShouldSessionRequestData(
                /*has_content*/ false,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ true));
  EXPECT_EQ(kNoRequestWithWait,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));

  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ false));
  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ no_refresh_large,
                /*has_outstanding_request*/ false));
  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ true));
  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ refresh_only_large,
                /*has_outstanding_request*/ true));

  EXPECT_EQ(
      kNoRequestWithTimeout,
      ShouldSessionRequestData(
          /*has_content*/ true, /*content_creation_date_time*/ stale_small,
          /*has_outstanding_request*/ true));
  EXPECT_EQ(kNoRequestWithTimeout,
            ShouldSessionRequestData(
                /*has_content*/ true, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));
}

TEST_F(FeedSchedulerHostTest, ShouldSessionRequestDataDivergentTimes) {
  // If a request fails, then the |content_creation_date_time| and the value of
  // prefs::kLastFetchAttemptTime may diverge. This is okay, and will typically
  // mean that refreshes are not taken. Staleness should continue to track
  // |content_creation_date_time|, but because staleness uses a bigger threshold
  // than NTP_OPEN, this will not affect much.

  // Like above case, the user is an kActiveNtpUser, staleness at 24 hours and
  // refresh at 4.
  Time refresh = test_clock()->Now() - TimeDelta::FromHours(5);
  Time no_refresh = test_clock()->Now() - TimeDelta::FromHours(3);
  Time stale = test_clock()->Now() - TimeDelta::FromHours(25);
  Time not_stale = test_clock()->Now() - TimeDelta::FromHours(23);

  ResetRefreshState(no_refresh);
  EXPECT_EQ(kNoRequestWithContent, scheduler()->ShouldSessionRequestData(
                                       /*has_content*/ true,
                                       /*content_creation_date_time*/ stale,
                                       /*has_outstanding_request*/ false));

  ResetRefreshState(refresh);
  EXPECT_EQ(kRequestWithContent, scheduler()->ShouldSessionRequestData(
                                     /*has_content*/ true,
                                     /*content_creation_date_time*/ not_stale,
                                     /*has_outstanding_request*/ false));

  ResetRefreshState(refresh);
  EXPECT_EQ(kRequestWithTimeout, scheduler()->ShouldSessionRequestData(
                                     /*has_content*/ true,
                                     /*content_creation_date_time*/ stale,
                                     /*has_outstanding_request*/ false));

  // This shouldn't be possible, since last attempt is farther back than
  // |content_creation_date_time| which updates on success, but verify scheduler
  // handles it reasonably.
  ResetRefreshState(Time());
  EXPECT_EQ(kRequestWithContent,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ false));

  // By changing the foregrounded threshold, staleness calculation changes.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"foregrounded_hours_active_ntp_user", "7.5"}});

  ResetRefreshState(Time());
  EXPECT_EQ(kRequestWithContent,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(7),
                /*has_outstanding_request*/ false));

  ResetRefreshState(Time());
  EXPECT_EQ(kRequestWithTimeout,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(8),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, NtpShownActiveNtpUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"ntp_shown_hours_active_ntp_user", "2.5"}});

  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(2),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(kRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(3),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, NtpShownRareNtpUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"ntp_shown_hours_rare_ntp_user", "1.5"}});

  ClassifyAsRareNtpUser();

  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(1),
                /*has_outstanding_request*/ false));

  // ShouldSessionRequestData() has the side effect of adding NTP_SHOWN event to
  // the classifier, so push the timer out to keep classification.
  ClassifyAsRareNtpUser();

  EXPECT_EQ(kRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(2),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, NtpShownActiveSuggestionsConsumer) {
  ClassifyAsActiveSuggestionsConsumer();

  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromMinutes(59),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(kRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromMinutes(61),
                /*has_outstanding_request*/ false));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"ntp_shown_hours_active_suggestions_consumer", "7.5"}});

  EXPECT_EQ(kNoRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(7),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(kRequestWithContent,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    TimeDelta::FromHours(8),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, OnReceiveNewContentVerifyPref) {
  EXPECT_EQ(Time(), profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(Time(), profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
  // Scheduler should prefer to use specified time over clock time.
  EXPECT_NE(test_clock()->Now(),
            profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnRequestErrorVerifyPref) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"only_set_last_refresh_attempt_on_success", "false"}});

  EXPECT_EQ(Time(), profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnRequestError(0);
  EXPECT_EQ(test_clock()->Now(),
            profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnRequestErrorVerifyRefreshTimeNotUpdated) {
  EXPECT_EQ(Time(), profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnRequestError(0);
  EXPECT_EQ(Time(), profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnForegroundedActiveNtpUser) {
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 24 hours.
  test_clock()->Advance(TimeDelta::FromHours(23));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(2));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"foregrounded_hours_active_ntp_user", "7.5"}});

  test_clock()->Advance(TimeDelta::FromHours(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnForegroundedRareNtpUser) {
  ClassifyAsRareNtpUser();

  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 24 hours.
  test_clock()->Advance(TimeDelta::FromHours(23));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(2));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"foregrounded_hours_rare_ntp_user", "7.5"}});

  test_clock()->Advance(TimeDelta::FromHours(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnForegroundedActiveSuggestionsConsumer) {
  ClassifyAsActiveSuggestionsConsumer();

  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 12 hours.
  test_clock()->Advance(TimeDelta::FromHours(11));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(2));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"foregrounded_hours_active_suggestions_consumer", "7.5"}});

  test_clock()->Advance(TimeDelta::FromHours(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerNullCallback) {
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerCompletionRunOnSuccess) {
  scheduler()->OnFixedTimer(base::BindOnce(
      &FeedSchedulerHostTest::FixedTimerCompletion, base::Unretained(this)));
  EXPECT_EQ(1, refresh_call_count());

  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(1, fixed_timer_completion_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerCompletionRunOnFailure) {
  scheduler()->OnFixedTimer(base::BindOnce(
      &FeedSchedulerHostTest::FixedTimerCompletion, base::Unretained(this)));
  EXPECT_EQ(1, refresh_call_count());

  scheduler()->OnRequestError(0);
  EXPECT_EQ(1, fixed_timer_completion_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerActiveNtpUser) {
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 48 hours.
  test_clock()->Advance(TimeDelta::FromHours(47));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(2));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"fixed_timer_hours_active_ntp_user", "7.5"}});

  test_clock()->Advance(TimeDelta::FromHours(7));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(1));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerActiveRareNtpUser) {
  ClassifyAsRareNtpUser();

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 96 hours.
  test_clock()->Advance(TimeDelta::FromHours(95));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(2));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"fixed_timer_hours_rare_ntp_user", "7.5"}});
  test_clock()->Advance(TimeDelta::FromHours(7));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(1));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerWhileHidden) {
  profile_prefs()->SetBoolean(prefs::kArticlesListVisible, false);
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(0, refresh_call_count());
  EXPECT_EQ(1, cancel_wake_up_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerActiveSuggestionsConsumer) {
  ClassifyAsActiveSuggestionsConsumer();

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 24 hours.
  test_clock()->Advance(TimeDelta::FromHours(23));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(2));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"fixed_timer_hours_active_suggestions_consumer", "7.5"}});

  test_clock()->Advance(TimeDelta::FromHours(7));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromHours(1));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerThrottlingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{kThrottleBackgroundFetches.name, "false"}});

  // Every call to OnFixedTimer() should cause a fetch.
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());
}

TEST_F(FeedSchedulerHostTest, ScheduleFixedTimerWakeUpOnSuccess) {
  // First wake up scheduled during Initialize().
  EXPECT_EQ(1U, schedule_wake_up_times().size());
  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(2U, schedule_wake_up_times().size());

  // Make another scheduler to initialize, make sure it doesn't schedule a
  // wake up.
  NewScheduler();
  EXPECT_EQ(2U, schedule_wake_up_times().size());
}

TEST_F(FeedSchedulerHostTest, InitializedIntoHidden) {
  // First wake up scheduled during Initialize().
  EXPECT_EQ(1U, schedule_wake_up_times().size());

  profile_prefs()->SetBoolean(prefs::kArticlesListVisible, false);
  profile_prefs()->ClearPref(prefs::kBackgroundRefreshPeriod);
  NewScheduler();
  EXPECT_EQ(1U, schedule_wake_up_times().size());
  EXPECT_EQ(0, cancel_wake_up_call_count());
}

TEST_F(FeedSchedulerHostTest, InitializedIntoHiddenWithPrevious) {
  // First wake up scheduled during Initialize().
  EXPECT_EQ(1U, schedule_wake_up_times().size());

  profile_prefs()->SetBoolean(prefs::kArticlesListVisible, false);
  profile_prefs()->SetTimeDelta(prefs::kBackgroundRefreshPeriod,
                                base::TimeDelta::FromDays(12345));
  NewScheduler();
  EXPECT_EQ(1U, schedule_wake_up_times().size());
  EXPECT_EQ(1, cancel_wake_up_call_count());
}

TEST_F(FeedSchedulerHostTest, InitializedIntoVisible) {
  // First wake up scheduled during Initialize().
  EXPECT_EQ(1U, schedule_wake_up_times().size());

  profile_prefs()->SetBoolean(prefs::kArticlesListVisible, true);
  profile_prefs()->ClearPref(prefs::kBackgroundRefreshPeriod);
  NewScheduler();
  EXPECT_EQ(2U, schedule_wake_up_times().size());
  EXPECT_EQ(0, cancel_wake_up_call_count());
}

TEST_F(FeedSchedulerHostTest, InitializedIntoVisibleWithPrevious) {
  // First wake up scheduled during Initialize().
  EXPECT_EQ(1U, schedule_wake_up_times().size());

  profile_prefs()->SetBoolean(prefs::kArticlesListVisible, true);
  profile_prefs()->SetTimeDelta(prefs::kBackgroundRefreshPeriod,
                                base::TimeDelta::FromDays(12345));
  NewScheduler();
  EXPECT_EQ(2U, schedule_wake_up_times().size());
  EXPECT_EQ(0, cancel_wake_up_call_count());
}

TEST_F(FeedSchedulerHostTest, ShouldRefreshOffline) {
  {
    ForceDeviceOffline forceDeviceOffline;
    scheduler()->OnForegrounded();
    EXPECT_EQ(0, refresh_call_count());
  }

  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, EulaNotAccepted) {
  if (PlatformSupportsEula()) {
    local_state()->SetBoolean(::prefs::kEulaAccepted, false);
    scheduler()->OnForegrounded();
    EXPECT_EQ(0, refresh_call_count());

    // The transition should kick off a refresh.
    local_state()->SetBoolean(::prefs::kEulaAccepted, true);
    EXPECT_EQ(1, refresh_call_count());

    // And now it doesn't block normal triggers either.
    ResetRefreshState(Time());
    scheduler()->OnForegrounded();
    EXPECT_EQ(2, refresh_call_count());
  }
}

TEST_F(FeedSchedulerHostTest, DisableOneTrigger) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{kDisableTriggerTypes.name, "foregrounded"}});
  NewScheduler();

  scheduler()->OnForegrounded();
  EXPECT_EQ(0, refresh_call_count());

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());

  EXPECT_EQ(kRequestWithWait,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, DisableAllTriggers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{kDisableTriggerTypes.name, "ntp_shown,foregrounded,fixed_timer"}});
  NewScheduler();

  scheduler()->OnForegrounded();
  EXPECT_EQ(0, refresh_call_count());

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(0, refresh_call_count());

  EXPECT_EQ(kNoRequestWithWait,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, DisableBogusTriggers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{kDisableTriggerTypes.name, "foo,123,#$*,,"}});
  NewScheduler();

  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  ResetRefreshState(Time());
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());

  EXPECT_EQ(kRequestWithWait,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, OnArticlesCleared) {
  // OnForegrounded() does nothing because content is fresher than threshold.
  scheduler()->OnReceiveNewContent(test_clock()->Now());
  scheduler()->OnForegrounded();
  EXPECT_EQ(0, refresh_call_count());

  EXPECT_FALSE(scheduler()->OnArticlesCleared(/*suppress_refreshes*/ true));

  scheduler()->OnForegrounded();
  EXPECT_EQ(0, refresh_call_count());

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(0, refresh_call_count());

  EXPECT_EQ(kNoRequestWithWait,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));

  test_clock()->Advance(TimeDelta::FromMinutes(29));

  scheduler()->OnForegrounded();
  EXPECT_EQ(0, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromMinutes(1));

  // Normally this would still be within foreground threshold, but the
  // OnHistoryCleared() cleared the last attempt time.
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, SuppressRefreshDuration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{kSuppressRefreshDurationMinutes.name, "100"}});
  EXPECT_FALSE(scheduler()->OnArticlesCleared(/*suppress_refreshes*/ true));

  test_clock()->Advance(TimeDelta::FromMinutes(99));
  scheduler()->OnForegrounded();
  EXPECT_EQ(0, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromMinutes(1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnArticlesClearedNoSuppress) {
  EXPECT_TRUE(scheduler()->OnArticlesCleared(/*suppress_refreshes*/ false));
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  EXPECT_TRUE(scheduler()->OnArticlesCleared(/*suppress_refreshes*/ false));
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  EXPECT_FALSE(scheduler()->OnArticlesCleared(/*suppress_refreshes*/ true));
  EXPECT_FALSE(scheduler()->OnArticlesCleared(/*suppress_refreshes*/ false));

  // OnArticlesCleared() should never trigger the refresh itself.
  EXPECT_EQ(0, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnArticlesClearedIgnoresOutstanding) {
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  // Now that there's an outstanding request, new triggers are not acted upon.
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  // Clearing articles should disregard the outstanding request logic.
  EXPECT_TRUE(scheduler()->OnArticlesCleared(/*suppress_refreshes*/ false));
  // OnArticlesCleared() should have returned true instead of triggering a
  // refresh directly.
  EXPECT_EQ(1, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OustandingRequest) {
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  scheduler()->OnForegrounded();
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  EXPECT_EQ(kNoRequestWithWait,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));

  profile_prefs()->SetTime(prefs::kLastFetchAttemptTime, base::Time());
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(
      TimeDelta::FromSeconds(kTimeoutDurationSeconds.Get() - 1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromSeconds(2));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  // OnReceiveNewContent() should also clear tracked outstanding request, but
  // similar to above, last attempted time is also set.
  scheduler()->OnReceiveNewContent(test_clock()->Now());
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(TimeDelta::FromDays(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(3, refresh_call_count());

  // Although this shouldn't typically happen, OnReceiveNewContent() takes a
  // time that could be wildly divergent from the scheduler's clock's now.
  scheduler()->OnReceiveNewContent(test_clock()->Now() -
                                   TimeDelta::FromDays(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(4, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, IncorporatesExternalOustandingRequest) {
  EXPECT_EQ(kNoRequestWithWait,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));

  // Normally this would trigger a refresh. In this case the scheduler will
  // notice the ShouldSessionRequestData() call carries information that there
  // is an outstanding request, which the scheduler should remember and then
  // prevent the OnForegrounded() from requesting a refresh.
  scheduler()->OnForegrounded();
  EXPECT_EQ(0, refresh_call_count());

  EXPECT_EQ(kRequestWithWait,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, IncorporatesExternalHasContent) {
  Time now = test_clock()->Now();
  EXPECT_EQ(Time(), profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));

  EXPECT_EQ(kNoRequestWithContent,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true, now, /*has_outstanding_request*/ false));
  EXPECT_EQ(now, profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));

  // Use has_outstanding_request of true to keep the scheduler from actually
  // triggering the refresh. We want to track the change to its internal state.
  EXPECT_EQ(kNoRequestWithWait, scheduler()->ShouldSessionRequestData(
                                    /*has_content*/ false, base::Time(),
                                    /*has_outstanding_request*/ true));
  EXPECT_EQ(Time(), profile_prefs()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, TimeUntilFirstMetrics) {
  base::HistogramTester histogram_tester;
  std::string ntpOpenedHistogram =
      "NewTabPage.ContentSuggestions.TimeUntilFirstShownTrigger.ActiveNTPUser";
  std::string forgroundedHistogram =
      "NewTabPage.ContentSuggestions.TimeUntilFirstStartupTrigger"
      ".ActiveNTPUser";
  Time now = test_clock()->Now();
  profile_prefs()->SetTime(prefs::kLastFetchAttemptTime, now);
  EXPECT_EQ(0U, histogram_tester.GetAllSamples(ntpOpenedHistogram).size());
  EXPECT_EQ(0U, histogram_tester.GetAllSamples(forgroundedHistogram).size());

  scheduler()->ShouldSessionRequestData(
      /*has_content*/ true, now, /*has_outstanding_request*/ false);
  EXPECT_EQ(1, histogram_tester.GetBucketCount(ntpOpenedHistogram, 0));
  EXPECT_EQ(0U, histogram_tester.GetAllSamples(forgroundedHistogram).size());

  scheduler()->OnForegrounded();
  EXPECT_EQ(1, histogram_tester.GetBucketCount(ntpOpenedHistogram, 0));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(forgroundedHistogram, 0));

  scheduler()->ShouldSessionRequestData(
      /*has_content*/ true, now, /*has_outstanding_request*/ false);
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, histogram_tester.GetBucketCount(ntpOpenedHistogram, 0));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(forgroundedHistogram, 0));

  // OnRequestError() should reset the flags, allowing these metrics to be
  // reported again.
  scheduler()->OnRequestError(0);

  scheduler()->ShouldSessionRequestData(
      /*has_content*/ true, now, /*has_outstanding_request*/ false);
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, histogram_tester.GetBucketCount(ntpOpenedHistogram, 0));
  EXPECT_EQ(2, histogram_tester.GetBucketCount(forgroundedHistogram, 0));
}

TEST_F(FeedSchedulerHostTest, RefreshThrottler) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedContentSuggestions,
      {{"quota_SuggestionFetcherActiveNTPUser", "3"}});
  NewScheduler();

  for (int i = 0; i < 5; i++) {
    scheduler()->OnForegrounded();
    ResetRefreshState(Time());
    EXPECT_EQ(std::min(i + 1, 3), refresh_call_count());
  }
}

TEST_F(FeedSchedulerHostTest, GetUserClassifierForDebuggingRareUser) {
  ClassifyAsRareNtpUser();

  EXPECT_EQ(UserClassifier::UserClass::kRareSuggestionsViewer,
            scheduler()->GetUserClassifierForDebugging()->GetUserClass());
}

TEST_F(FeedSchedulerHostTest, GetUserClassifierForDebuggingActiveConsumer) {
  ClassifyAsActiveSuggestionsConsumer();

  EXPECT_EQ(UserClassifier::UserClass::kActiveSuggestionsConsumer,
            scheduler()->GetUserClassifierForDebugging()->GetUserClass());
}

TEST_F(FeedSchedulerHostTest, GetSuppressRefreshesUntilForDebugging) {
  EXPECT_TRUE(scheduler()->GetSuppressRefreshesUntilForDebugging().is_null());

  scheduler()->OnArticlesCleared(/*suppress_refreshes*/ true);

  EXPECT_EQ(test_clock()->Now() + TimeDelta::FromMinutes(30),
            scheduler()->GetSuppressRefreshesUntilForDebugging());
}

TEST_F(FeedSchedulerHostTest, GetLastFetchStatusForDebugging) {
  EXPECT_EQ(0, scheduler()->GetLastFetchStatusForDebugging());

  scheduler()->OnReceiveNewContent(Time());

  EXPECT_EQ(200, scheduler()->GetLastFetchStatusForDebugging());

  scheduler()->OnRequestError(-100);

  EXPECT_EQ(-100, scheduler()->GetLastFetchStatusForDebugging());
}

TEST_F(FeedSchedulerHostTest, GetLastFetchTriggerTypeForDebugging) {
  scheduler()->OnForegrounded();

  EXPECT_EQ(FeedSchedulerHost::TriggerType::kForegrounded,
            *scheduler()->GetLastFetchTriggerTypeForDebugging());

  scheduler()->OnArticlesCleared(/*suppress_refreshes*/ false);

  EXPECT_EQ(FeedSchedulerHost::TriggerType::kNtpShown,
            *scheduler()->GetLastFetchTriggerTypeForDebugging());

  ClassifyAsActiveSuggestionsConsumer();  // Fixed timer at 48 hours.
  test_clock()->Advance(TimeDelta::FromHours(49));
  scheduler()->OnFixedTimer(base::OnceClosure());

  EXPECT_EQ(FeedSchedulerHost::TriggerType::kFixedTimer,
            *scheduler()->GetLastFetchTriggerTypeForDebugging());
}

}  // namespace feed
