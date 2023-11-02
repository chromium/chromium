// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/appid_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace cfm {
namespace {

using CfmAppIdUtilTest = testing::Test;

TEST_F(CfmAppIdUtilTest, AppIdIsTrue) {
  std::string app_id = "hkamnlhnogggfddmjomgbdokdkgfelgg";
  ASSERT_TRUE(IsChromeboxForMeetingsAppId(app_id));
}

TEST_F(CfmAppIdUtilTest, AppIdIsFalse) {
  std::string app_id = "FAKE_APP_ID";
  ASSERT_FALSE(IsChromeboxForMeetingsAppId(app_id));
}

}  // namespace
}  // namespace cfm
}  // namespace ash
