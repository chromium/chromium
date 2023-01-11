// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/dispatcher.h"

#include "base/functional/bind.h"
#include "components/domain_reliability/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

using base::TimeTicks;

class DomainReliabilityDispatcherTest : public testing::Test {
 public:
  DomainReliabilityDispatcherTest() : dispatcher_(&time_) {}

 protected:
  MockTime time_;
  DomainReliabilityDispatcher dispatcher_;
};

TEST_F(DomainReliabilityDispatcherTest, Create) {
}

TEST_F(DomainReliabilityDispatcherTest, TaskDoesntRunEarly) {
  base::TimeDelta delay = base::Seconds(1);
  TestCallback callback;

  dispatcher_.ScheduleTask(callback.callback(), 2 * delay, 3 * delay);
  time_.Advance(delay);
  dispatcher_.RunEligibleTasks();
  EXPECT_FALSE(callback.called());
}

TEST_F(DomainReliabilityDispatcherTest, TaskRunsWhenEligible) {
  base::TimeDelta delay = base::Seconds(1);
  TestCallback callback;

  dispatcher_.ScheduleTask(callback.callback(), 2 * delay, 3 * delay);
  time_.Advance(2 * delay);
  EXPECT_FALSE(callback.called());
  dispatcher_.RunEligibleTasks();
  EXPECT_TRUE(callback.called());
  time_.Advance(delay);
}

TEST_F(DomainReliabilityDispatcherTest, TaskRunsAtDeadline) {
  base::TimeDelta delay = base::Seconds(1);
  TestCallback callback;

  dispatcher_.ScheduleTask(callback.callback(), 2 * delay, 3 * delay);
  time_.Advance(2 * delay);
  EXPECT_FALSE(callback.called());
  time_.Advance(delay);
  EXPECT_TRUE(callback.called());
}

}  // namespace
}  // namespace domain_reliability
