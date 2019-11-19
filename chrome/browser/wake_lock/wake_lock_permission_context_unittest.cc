// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wake_lock/wake_lock_permission_context.h"

#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WakeLockPermissionContextTests : public testing::Test {
 public:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(WakeLockPermissionContextTests, InsecureOriginsAreRejected) {
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  const ContentSettingsType kWakeLockTypes[] = {
      ContentSettingsType::WAKE_LOCK_SCREEN,
      ContentSettingsType::WAKE_LOCK_SYSTEM};

  for (const auto& content_settings_type : kWakeLockTypes) {
    WakeLockPermissionContext permission_context(profile(),
                                                 content_settings_type);
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              permission_context
                  .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                       insecure_url, insecure_url)
                  .content_setting);
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              permission_context
                  .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                       insecure_url, secure_url)
                  .content_setting);
  }
}

TEST_F(WakeLockPermissionContextTests, TestScreenLockPermissionRequest) {
  WakeLockPermissionContext permission_context(
      profile(), ContentSettingsType::WAKE_LOCK_SCREEN);
  GURL url("https://www.example.com");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .content_setting);
}

TEST_F(WakeLockPermissionContextTests, TestSystemLockPermissionRequest) {
  WakeLockPermissionContext permission_context(
      profile(), ContentSettingsType::WAKE_LOCK_SYSTEM);
  GURL url("https://www.example.com");
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .content_setting);
}

}  // namespace
