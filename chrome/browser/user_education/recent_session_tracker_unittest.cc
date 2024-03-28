// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/recent_session_tracker.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/test/test_feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// At some point these may need to be moved to a common header file.

// Short-circuits all of the functionality of the session manager except for
// updating session data and sending notifications.
class FakeSessionManager
    : protected user_education::FeaturePromoSessionManager {
 public:
  explicit FakeSessionManager(
      user_education::FeaturePromoStorageService& storage_service)
      : storage_service_(storage_service) {}
  ~FakeSessionManager() override = default;

  // Simulates a new session at the current time provided by the storage
  // service's clock.
  void SimulateNewSession() {
    const auto old_session = storage_service_->ReadSessionData();
    const auto now = storage_service_->GetCurrentTime();
    user_education::FeaturePromoSessionData new_session;
    new_session.most_recent_active_time = now;
    new_session.start_time = now;
    storage_service_->SaveSessionData(new_session);
    OnNewSession(old_session.start_time, old_session.most_recent_active_time,
                 now);
  }

  user_education::FeaturePromoSessionManager& AsSessionManager() {
    return *static_cast<user_education::FeaturePromoSessionManager*>(this);
  }

 private:
  const raw_ref<user_education::FeaturePromoStorageService> storage_service_;
};

// Implementation of `RecentSessionDataStorageService` that extends
// `TestFeaturePromoStorageService` with in-memory data storage.
class TestBrowserStorageService
    : public user_education::test::TestFeaturePromoStorageService,
      public RecentSessionDataStorageService {
 public:
  TestBrowserStorageService() = default;
  ~TestBrowserStorageService() override = default;

  RecentSessionData ReadRecentSessionData() const override {
    RecentSessionData data;
    data.recent_session_start_times = recent_sessions_;
    data.enabled_time = enabled_time_;
    return data;
  }

  void SaveRecentSessionData(const RecentSessionData& data) override {
    recent_sessions_ = data.recent_session_start_times;
    enabled_time_ = data.enabled_time;
  }

 private:
  std::vector<base::Time> recent_sessions_;
  std::optional<base::Time> enabled_time_;
};

base::Time Days(double d) {
  return base::Time::FromSecondsSinceUnixEpoch(d * 24 * 60 * 60);
}

}  // namespace

class RecentSessionTrackerTest : public testing::Test {
 public:
  RecentSessionTrackerTest() {
    storage_service_.set_clock_for_testing(&clock_);
  }
  ~RecentSessionTrackerTest() override = default;

  // Sets up the data for the test.
  //  - saves a list of `old_sessions` to the storage service
  //  - optionally starts a new session in the current process
  //  - creates the tracker and initializes it
  //
  // Use `session_before_tracker_init` if you want to simulate a session
  // starting before the tracker is created; otherwise you can call
  // `StartNewSession()` to create a new session later.
  void Init(const std::vector<base::Time>& old_sessions,
            std::optional<base::Time> session_before_tracker_init) {
    RecentSessionData data;
    data.recent_session_start_times = old_sessions;
    if (!old_sessions.empty()) {
      data.enabled_time = old_sessions.back();
    }
    storage_service_.SaveRecentSessionData(data);
    if (session_before_tracker_init) {
      StartNewSession(*session_before_tracker_init);
    }
    tracker_ = std::make_unique<RecentSessionTracker>(
        session_manager_.AsSessionManager(), storage_service_,
        storage_service_);
    update_subscription_ = tracker_->AddRecentSessionsUpdatedCallback(
        base::BindLambdaForTesting([this](const RecentSessionData& data) {
          ++update_count_;
          last_update_data_ = data;
        }));
  }

  // Starts a new session at `session_time`.
  void StartNewSession(base::Time session_time) {
    clock_.SetNow(session_time);
    session_manager_.SimulateNewSession();
  }

  void EnsureRecentSessions(const std::vector<base::Time>& expected_data,
                            int expected_update_count) const {
    const auto recent_sessions = storage_service_.ReadRecentSessionData();
    EXPECT_THAT(recent_sessions.recent_session_start_times,
                testing::ContainerEq(expected_data));
    EXPECT_EQ(expected_update_count, update_count_);
    if (last_update_data_) {
      EXPECT_THAT(last_update_data_->recent_session_start_times,
                  testing::ContainerEq(expected_data));
    }
    EXPECT_EQ(expected_update_count > 0,
              tracker_->recent_session_data_for_testing().has_value());
    if (tracker_->recent_session_data_for_testing().has_value()) {
      EXPECT_THAT(tracker_->recent_session_data_for_testing()
                      ->recent_session_start_times,
                  testing::ContainerEq(expected_data));
    }
    ASSERT_EQ(!recent_sessions.recent_session_start_times.empty(),
              recent_sessions.enabled_time.has_value());
    if (recent_sessions.enabled_time) {
      EXPECT_LE(*recent_sessions.enabled_time,
                recent_sessions.recent_session_start_times.back());
    }
  }

 private:
  base::SimpleTestClock clock_;
  TestBrowserStorageService storage_service_;
  FakeSessionManager session_manager_{storage_service_};
  std::unique_ptr<RecentSessionTracker> tracker_;
  int update_count_ = 0;
  std::optional<RecentSessionData> last_update_data_;
  base::CallbackListSubscription update_subscription_;
};

TEST_F(RecentSessionTrackerTest, NoNewSession) {
  const std::vector<base::Time> old_sessions{
      Days(102),
      Days(101),
      Days(100),
  };
  Init(old_sessions, std::nullopt);
  EnsureRecentSessions(old_sessions, 0);
}

TEST_F(RecentSessionTrackerTest, NewSessionBeforeInit) {
  std::vector<base::Time> sessions{
      Days(102),
      Days(101),
      Days(100),
  };
  Init(sessions, Days(103));
  sessions.insert(sessions.begin(), Days(103));
  EnsureRecentSessions(sessions, 1);
}

TEST_F(RecentSessionTrackerTest, NewSessionAfterInit) {
  std::vector<base::Time> sessions{
      Days(102),
      Days(101),
      Days(100),
  };
  Init(sessions, std::nullopt);
  StartNewSession(Days(103));
  sessions.insert(sessions.begin(), Days(103));
  EnsureRecentSessions(sessions, 1);
}

TEST_F(RecentSessionTrackerTest, OldSessionsRollOff) {
  std::vector<base::Time> sessions{
      Days(103),
      Days(102),
      Days(101),
      Days(100),
  };

  // Move forward enough that the first two entries roll out of the window.
  const base::Time time1 = Days(102) +
                           RecentSessionTracker::kMaxRecentSessionRetention -
                           base::Seconds(1);
  Init(sessions, time1);
  sessions = {
      time1,
      Days(103),
      Days(102),
  };
  EnsureRecentSessions(sessions, 1);

  // Move forward enough to cause the next entry to roll off.
  const base::Time time2 = time1 + base::Seconds(2);
  StartNewSession(time2);
  sessions = {
      time2,
      time1,
      Days(103),
  };
  EnsureRecentSessions(sessions, 2);

  // Move forward enough to cause all entries but the current one to roll off.
  const base::Time time3 = time2 +
                           RecentSessionTracker::kMaxRecentSessionRetention +
                           base::Seconds(1);
  StartNewSession(time3);
  sessions = {time3};
  EnsureRecentSessions(sessions, 3);
}

TEST_F(RecentSessionTrackerTest, TooManySessionsRollOff) {
  // Create a list of sessions of max length.
  std::vector<base::Time> sessions;
  for (int i = RecentSessionTracker::kMaxRecentSessionRecords; i > 0; i--) {
    sessions.push_back(Days(100 + i));
  }

  // Add a new session; the last session should roll off.
  const base::Time time1 = sessions.front() + base::Days(1);
  Init(sessions, time1);
  sessions.pop_back();
  sessions.insert(sessions.begin(), time1);
  EnsureRecentSessions(sessions, 1);

  // Add a new session; the last session should roll off.
  const base::Time time2 = time1 + base::Days(1);
  StartNewSession(time2);
  sessions.pop_back();
  sessions.insert(sessions.begin(), time2);
  EnsureRecentSessions(sessions, 2);
}

TEST_F(RecentSessionTrackerTest, ClockSetBackElidesValues) {
  std::vector<base::Time> sessions{
      Days(104),
      Days(103),
      Days(101),
      Days(100),
  };
  Init(sessions, Days(102));
  sessions = {
      Days(102),
      Days(101),
      Days(100),
  };
  EnsureRecentSessions(sessions, 1);
}

TEST_F(RecentSessionTrackerTest, EnabledForTheFirstTime) {
  Init({}, std::nullopt);
  EnsureRecentSessions({}, 0);
  StartNewSession(Days(100));
  EnsureRecentSessions({Days(100)}, 1);
}
