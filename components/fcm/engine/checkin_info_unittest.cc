// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/engine/checkin_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace fcm {
namespace {

TEST(CheckinInfoTest, IsValid) {
  CheckinInfo info;
  EXPECT_FALSE(info.IsValid());

  info.set_android_id(12345ULL);
  EXPECT_FALSE(info.IsValid());

  info.set_secret(67890ULL);
  EXPECT_TRUE(info.IsValid());

  info.set_android_id(0);
  EXPECT_FALSE(info.IsValid());
}

TEST(CheckinInfoTest, Reset) {
  CheckinInfo info(12345ULL, 67890ULL);
  info.set_accounts_set(true);
  ASSERT_TRUE(info.IsValid());

  info.Reset();
  EXPECT_FALSE(info.IsValid());
  EXPECT_EQ(0u, info.android_id());
  EXPECT_EQ(0u, info.secret());
  EXPECT_FALSE(info.accounts_set());
}

}  // namespace
}  // namespace fcm
