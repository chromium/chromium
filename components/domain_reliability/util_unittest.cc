// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/util.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "components/domain_reliability/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

using base::TimeTicks;

class DomainReliabilityMockTimeTest : public testing::Test {
 protected:
  MockTime time_;
};

TEST_F(DomainReliabilityMockTimeTest, Create) {
}

TEST_F(DomainReliabilityMockTimeTest, NowAndAdvance) {
  const base::TimeDelta delta = base::Seconds(1);

  TimeTicks initial = time_.NowTicks();
  time_.Advance(delta);
  TimeTicks final = time_.NowTicks();
  EXPECT_EQ(delta, final - initial);
}

TEST_F(DomainReliabilityMockTimeTest, AddTask) {
  const base::TimeDelta delta = base::Seconds(1);
  TestCallback callback;

  time_.AddTask(2 * delta, callback.callback());
  time_.Advance(delta);
  EXPECT_FALSE(callback.called());
  time_.Advance(delta);
  EXPECT_TRUE(callback.called());
}

TEST_F(DomainReliabilityMockTimeTest, TimerCreate) {
  std::unique_ptr<MockTime::Timer> timer(time_.CreateTimer());
}

TEST_F(DomainReliabilityMockTimeTest, TimerIsRunning) {
  const base::TimeDelta delta = base::Seconds(1);
  TestCallback callback;

  std::unique_ptr<MockTime::Timer> timer(time_.CreateTimer());
  EXPECT_FALSE(timer->IsRunning());
  timer->Start(FROM_HERE, delta, callback.callback());
  EXPECT_TRUE(timer->IsRunning());
  timer->Stop();
  EXPECT_FALSE(timer->IsRunning());
}

TEST_F(DomainReliabilityMockTimeTest, TimerGoesOff) {
  const base::TimeDelta delta = base::Seconds(1);
  TestCallback callback;

  std::unique_ptr<MockTime::Timer> timer(time_.CreateTimer());

  timer->Start(FROM_HERE, 2 * delta, callback.callback());
  time_.Advance(delta);
  EXPECT_FALSE(callback.called());
  time_.Advance(delta);
  EXPECT_TRUE(callback.called());
}

TEST_F(DomainReliabilityMockTimeTest, TimerStopped) {
  const base::TimeDelta delta = base::Seconds(1);
  TestCallback callback;

  std::unique_ptr<MockTime::Timer> timer(time_.CreateTimer());

  timer->Start(FROM_HERE, 2 * delta, callback.callback());
  time_.Advance(delta);
  timer->Stop();
  time_.Advance(delta);
  EXPECT_FALSE(callback.called());
}

TEST_F(DomainReliabilityMockTimeTest, TimerRestarted) {
  const base::TimeDelta delta = base::Seconds(1);
  TestCallback callback;

  std::unique_ptr<MockTime::Timer> timer(time_.CreateTimer());

  timer->Start(FROM_HERE, 2 * delta, callback.callback());
  time_.Advance(delta);
  timer->Start(FROM_HERE, 2 * delta, callback.callback());
  time_.Advance(delta);
  EXPECT_FALSE(callback.called());
  time_.Advance(delta);
  EXPECT_TRUE(callback.called());
}

TEST_F(DomainReliabilityMockTimeTest, TimerReentrantStart) {
  const base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<MockTime::Timer> timer(time_.CreateTimer());
  TestCallback callback;

  timer->Start(
      FROM_HERE, delta,
      base::BindOnce(&MockTime::Timer::Start, base::Unretained(timer.get()),
                     FROM_HERE, delta, callback.callback()));
  time_.Advance(delta);
  EXPECT_FALSE(callback.called());
  EXPECT_TRUE(timer->IsRunning());
  time_.Advance(delta);
  EXPECT_TRUE(callback.called());
  EXPECT_FALSE(timer->IsRunning());
}

}  // namespace
}  // namespace domain_reliability
