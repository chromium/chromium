// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/timeout.h"
#include "base/check_op.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"


TEST(TimeoutTest, Basics) {
  Timeout timeout;
  EXPECT_FALSE(timeout.is_set());
  EXPECT_FALSE(timeout.IsExpired());
  EXPECT_EQ(base::TimeDelta::Max(), timeout.GetDuration());
  EXPECT_EQ(base::TimeDelta::Max(), timeout.GetRemainingTime());

  timeout.SetDuration(base::TimeDelta());
  EXPECT_TRUE(timeout.is_set());
  EXPECT_TRUE(timeout.IsExpired());
  EXPECT_EQ(base::TimeDelta(), timeout.GetDuration());
  EXPECT_GE(base::TimeDelta(), timeout.GetRemainingTime());
}

TEST(TimeoutTest, SetDuration) {
  Timeout timeout(base::Seconds(1));

  // It's ok to set the same duration again, since nothing changes.
  timeout.SetDuration(base::Seconds(1));

  EXPECT_DCHECK_DEATH(timeout.SetDuration(base::Minutes(30)));
}

TEST(TimeoutTest, Derive) {
  Timeout timeout(base::Minutes(5));
  EXPECT_TRUE(timeout.is_set());
  EXPECT_FALSE(timeout.IsExpired());
  EXPECT_EQ(base::Minutes(5), timeout.GetDuration());
  EXPECT_GE(base::Minutes(5), timeout.GetRemainingTime());

  Timeout small = Timeout(base::Seconds(10), &timeout);
  EXPECT_TRUE(small.is_set());
  EXPECT_FALSE(small.IsExpired());
  EXPECT_EQ(base::Seconds(10), small.GetDuration());

  Timeout large = Timeout(base::Minutes(30), &timeout);
  EXPECT_TRUE(large.is_set());
  EXPECT_FALSE(large.IsExpired());
  EXPECT_GE(timeout.GetDuration(), large.GetDuration());
}

TEST(TimeoutTest, DeriveExpired) {
  Timeout timeout((base::TimeDelta()));
  EXPECT_TRUE(timeout.is_set());
  EXPECT_TRUE(timeout.IsExpired());

  Timeout derived = Timeout(base::Seconds(10), &timeout);
  EXPECT_TRUE(derived.is_set());
  EXPECT_TRUE(derived.IsExpired());
  EXPECT_GE(base::TimeDelta(), derived.GetDuration());
}
