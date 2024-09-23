// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp/appid_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace cfm {
namespace {

using CfmAppIdUtilTest = testing::Test;

TEST_F(CfmAppIdUtilTest, InternalHashedAppIdIsTrue) {
  std::string hashed_app_id = "81986D4F846CEDDDB962643FA501D1780DD441BB";
  ASSERT_TRUE(IsChromeboxForMeetingsHashedAppId(hashed_app_id));
}

TEST_F(CfmAppIdUtilTest, ExternalHashedAppIdIsTrue) {
  std::string hashed_app_id = "A9A9FC0228ADF541F0334F22BEFB8F9C245B21D7";
  ASSERT_TRUE(IsChromeboxForMeetingsHashedAppId(hashed_app_id));
}

TEST_F(CfmAppIdUtilTest, HashedAppIdIsFalse) {
  std::string hashed_app_id = "FAKE_APP_ID";
  ASSERT_FALSE(IsChromeboxForMeetingsHashedAppId(hashed_app_id));
}

}  // namespace
}  // namespace cfm
}  // namespace chromeos
