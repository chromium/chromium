// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/system_time_provider.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

base::Time GetTime(int year, int month, int day) {
  const base::Time::Exploded exploded_time = {
      .year = year, .month = month, .day_of_month = day};
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded_time, &out_time));
  return out_time;
}

// A SystemTimeProvider where the current time can be defined at runtime.
class TestSystemTimeProvider : public SystemTimeProvider {
 public:
  TestSystemTimeProvider() = default;

  TestSystemTimeProvider(const TestSystemTimeProvider&) = delete;
  TestSystemTimeProvider& operator=(const TestSystemTimeProvider&) = delete;

  // SystemTimeProvider implementation.
  base::Time Now() const override { return current_time_; }

  void SetCurrentTime(base::Time time) { current_time_ = time; }

 private:
  base::Time current_time_;
};

class SystemTimeProviderTest : public ::testing::Test {
 public:
  SystemTimeProviderTest() = default;

  SystemTimeProviderTest(const SystemTimeProviderTest&) = delete;
  SystemTimeProviderTest& operator=(const SystemTimeProviderTest&) = delete;

 protected:
  TestSystemTimeProvider time_provider_;
};

}  // namespace

TEST_F(SystemTimeProviderTest, EpochIs0Days) {
  time_provider_.SetCurrentTime(base::Time::UnixEpoch());
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestDeltasFromEpoch) {
  base::Time epoch = base::Time::UnixEpoch();

  time_provider_.SetCurrentTime(epoch + base::Days(1));
  EXPECT_EQ(1u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch + base::Days(2));
  EXPECT_EQ(2u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch + base::Days(100));
  EXPECT_EQ(100u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestNegativeDeltasFromEpoch) {
  base::Time epoch = base::Time::UnixEpoch();

  time_provider_.SetCurrentTime(epoch - base::Days(1));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch - base::Days(2));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch - base::Days(100));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestManualDatesAroundEpoch) {
  time_provider_.SetCurrentTime(GetTime(1970, 1, 1));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(1970, 1, 2));
  EXPECT_EQ(1u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(1970, 4, 11));
  EXPECT_EQ(100u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestManualDatesAroundGoogleIO2017) {
  time_provider_.SetCurrentTime(GetTime(2017, 5, 17));
  EXPECT_EQ(17303u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(2017, 5, 18));
  EXPECT_EQ(17304u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(2017, 5, 19));
  EXPECT_EQ(17305u, time_provider_.GetCurrentDay());
}

}  // namespace feature_engagement
