// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

const base::Time kTime0 = base::Time();
const base::Time kTime1 = base::Time::FromTimeT(1);
const base::Time kTime2 = base::Time::FromTimeT(2);

TEST(FlocIdTest, IsValid) {
  EXPECT_FALSE(FlocId().IsValid());
  EXPECT_TRUE(FlocId(0, kTime0, kTime0, 0).IsValid());
  EXPECT_TRUE(FlocId(0, kTime1, kTime2, 1).IsValid());
}

TEST(FlocIdTest, Comparison) {
  EXPECT_EQ(FlocId(), FlocId());

  EXPECT_EQ(FlocId(0, kTime0, kTime0, 0), FlocId(0, kTime0, kTime0, 0));
  EXPECT_EQ(FlocId(0, kTime1, kTime1, 1), FlocId(0, kTime1, kTime1, 1));
  EXPECT_EQ(FlocId(0, kTime1, kTime2, 1), FlocId(0, kTime1, kTime2, 1));

  EXPECT_NE(FlocId(), FlocId(0, kTime0, kTime0, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime0, 0), FlocId(1, kTime0, kTime0, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime1, 0), FlocId(0, kTime1, kTime1, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime0, 0), FlocId(0, kTime0, kTime0, 1));
}

TEST(FlocIdTest, ToStringForJsApi) {
  EXPECT_EQ("0.1.0", FlocId(0, kTime0, kTime0, 0).ToStringForJsApi());
  EXPECT_EQ("12345.1.0", FlocId(12345, kTime0, kTime0, 0).ToStringForJsApi());
  EXPECT_EQ("12345.1.2", FlocId(12345, kTime1, kTime1, 2).ToStringForJsApi());
}

}  // namespace federated_learning
