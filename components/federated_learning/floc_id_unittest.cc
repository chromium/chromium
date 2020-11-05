// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

TEST(FlocIdTest, IsValid) {
  EXPECT_FALSE(FlocId().IsValid());
  EXPECT_TRUE(FlocId(0, 0).IsValid());
  EXPECT_TRUE(FlocId(0, 1).IsValid());
}

TEST(FlocIdTest, ToUint64) {
  EXPECT_EQ(0u, FlocId(0, 0).ToUint64());
  EXPECT_EQ(1u, FlocId(1, 0).ToUint64());
  EXPECT_EQ(1u, FlocId(1, 1).ToUint64());
}

TEST(FlocIdTest, Comparison) {
  EXPECT_EQ(FlocId(), FlocId());
  EXPECT_EQ(FlocId(0, 0), FlocId(0, 0));
  EXPECT_EQ(FlocId(0, 1), FlocId(0, 1));

  EXPECT_NE(FlocId(), FlocId(0, 0));
  EXPECT_NE(FlocId(0, 0), FlocId(1, 0));
  EXPECT_NE(FlocId(0, 0), FlocId(0, 1));
}

TEST(FlocIdTest, ToString) {
  EXPECT_EQ("0.1.0", FlocId(0, 0).ToString());
  EXPECT_EQ("12345.1.0", FlocId(12345, 0).ToString());
  EXPECT_EQ("12345.1.2", FlocId(12345, 2).ToString());
}

}  // namespace federated_learning
