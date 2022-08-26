// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/review_notification_permissions_helper.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_settings {

class ReviewNotificationPermissionsHelperTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ReviewNotificationPermissionsHelperTest,
       CheckReviewNotificationPermissions) {
  auto notification_permissions = GetReviewNotificationPermissions(profile());
  { EXPECT_EQ(3UL, notification_permissions.size()); }
}

}  // namespace site_settings
