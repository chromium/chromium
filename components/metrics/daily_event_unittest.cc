// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/daily_event.h"

#include <optional>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const char kTestPrefName[] = "TestPref";
const char kTestMetricName[] = "TestMetric";

class TestDailyObserver : public DailyEvent::Observer {
 public:
  TestDailyObserver() = default;

  TestDailyObserver(const TestDailyObserver&) = delete;
  TestDailyObserver& operator=(const TestDailyObserver&) = delete;

  bool fired() const { return type_.has_value(); }
  DailyEvent::IntervalType type() const { return type_.value(); }

  void OnDailyEvent(DailyEvent::IntervalType type) override { type_ = type; }

  void Reset() { type_ = {}; }

 private:
  // Last-received type, or unset if OnDailyEvent() hasn't been called.
  std::optional<DailyEvent::IntervalType> type_;
};

class DailyEventTest : public testing::Test {
 public:
  DailyEventTest() : event_(&prefs_, kTestPrefName, kTestMetricName) {
    DailyEvent::RegisterPref(prefs_.registry(), kTestPrefName);
    auto observer = std::make_unique<TestDailyObserver>();
    observer_ = observer.get();
    event_.AddObserver(std::move(observer));
  }

  DailyEventTest(const DailyEventTest&) = delete;
  DailyEventTest& operator=(const DailyEventTest&) = delete;

 protected:
  TestingPrefServiceSimple prefs_;
  DailyEvent event_;  // Owns and outlives `observer_`
  raw_ptr<TestDailyObserver> observer_;
  base::test::TaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

}  // namespace

// The event should fire if the preference is not available.
TEST_F(DailyEventTest, TestNewFires) {
  event_.CheckInterval();
  ASSERT_TRUE(observer_->fired());
  EXPECT_EQ(DailyEvent::IntervalType::FIRST_RUN, observer_->type());
}

// The event should fire if the preference is more than a day old.
TEST_F(DailyEventTest, TestOldFires) {
  base::Time last_time = base::Time::Now() - base::Hours(25);
  prefs_.SetInt64(kTestPrefName, last_time.since_origin().InMicroseconds());
  event_.CheckInterval();
  ASSERT_TRUE(observer_->fired());
  EXPECT_EQ(DailyEvent::IntervalType::DAY_ELAPSED, observer_->type());
}

// The event should fire if the preference is more than a day in the future.
TEST_F(DailyEventTest, TestFutureFires) {
  base::Time last_time = base::Time::Now() + base::Hours(25);
  prefs_.SetInt64(kTestPrefName, last_time.since_origin().InMicroseconds());
  event_.CheckInterval();
  ASSERT_TRUE(observer_->fired());
  EXPECT_EQ(DailyEvent::IntervalType::CLOCK_CHANGED, observer_->type());
}

// The event should not fire if the preference is more recent than a day.
TEST_F(DailyEventTest, TestRecentNotFired) {
  base::Time last_time = base::Time::Now() - base::Minutes(2);
  prefs_.SetInt64(kTestPrefName, last_time.since_origin().InMicroseconds());
  event_.CheckInterval();
  EXPECT_FALSE(observer_->fired());
}

// The event should not fire if the preference is less than a day in the future.
TEST_F(DailyEventTest, TestSoonNotFired) {
  base::Time last_time = base::Time::Now() + base::Minutes(2);
  prefs_.SetInt64(kTestPrefName, last_time.since_origin().InMicroseconds());
  event_.CheckInterval();
  EXPECT_FALSE(observer_->fired());
}

void TestCallback(bool* fired) {
  *fired = true;
}

TEST_F(DailyEventTest, TestClosureFired) {
  // Verify the event fires on the first check.
  bool fired = false;
  event_.AddObserverClosure(base::BindRepeating(&TestCallback, &fired));
  event_.CheckInterval();
  EXPECT_TRUE(fired);
  EXPECT_EQ(DailyEvent::IntervalType::FIRST_RUN, observer_->type());

  // Reset the flag. A check on the same day should not fire the event.
  fired = false;
  event_.CheckInterval();
  EXPECT_FALSE(fired);

  // Advance time by 25 hours to verify the event fires again.
  env_.FastForwardBy(base::Hours(25));
  event_.CheckInterval();
  EXPECT_TRUE(fired);
  EXPECT_EQ(DailyEvent::IntervalType::DAY_ELAPSED, observer_->type());
}

}  // namespace metrics
