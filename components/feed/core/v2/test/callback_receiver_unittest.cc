// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/callback_receiver.h"

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

TEST(CallbackReceiverTest, OneResult) {
  CallbackReceiver<int> cr1;
  cr1.Done(42);

  ASSERT_NE(cr1.GetResult(), base::nullopt);
  EXPECT_EQ(*cr1.GetResult(), 42);
  EXPECT_EQ(*cr1.GetResult<0>(), 42);
  EXPECT_EQ(*cr1.GetResult<int>(), 42);
}

TEST(CallbackReceiverTest, MultipleResults) {
  CallbackReceiver<std::string, bool> cr2;
  EXPECT_EQ(cr2.GetResult<0>(), base::nullopt);
  EXPECT_EQ(cr2.GetResult<1>(), base::nullopt);
  cr2.Done("asdfasdfasdf", false);

  ASSERT_NE(cr2.GetResult<0>(), base::nullopt);
  EXPECT_EQ(*cr2.GetResult<0>(), "asdfasdfasdf");
  EXPECT_EQ(*cr2.GetResult<std::string>(), "asdfasdfasdf");
  ASSERT_NE(cr2.GetResult<1>(), base::nullopt);
  EXPECT_EQ(*cr2.GetResult<1>(), false);
  EXPECT_EQ(*cr2.GetResult<bool>(), false);
}

TEST(CallbackReceiverTest, Clear) {
  CallbackReceiver<int, bool> cr;
  cr.Done(10, true);
  cr.Clear();
  EXPECT_EQ(cr.GetResult<0>(), base::nullopt);
  EXPECT_EQ(cr.GetResult<1>(), base::nullopt);
}

}  // namespace feed
