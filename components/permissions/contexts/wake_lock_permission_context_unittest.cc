// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/wake_lock_permission_context.h"

#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

using PermissionStatus = blink::mojom::PermissionStatus;

class WakeLockPermissionContextTests : public testing::Test {
 public:
  content::TestBrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestPermissionsClient client_;
};

TEST_F(WakeLockPermissionContextTests, InsecureOriginsAreRejected) {
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  const ContentSettingsType kWakeLockTypes[] = {
      ContentSettingsType::WAKE_LOCK_SCREEN,
      ContentSettingsType::WAKE_LOCK_SYSTEM};

  for (const auto& content_settings_type : kWakeLockTypes) {
    WakeLockPermissionContext permission_context(browser_context(),
                                                 content_settings_type);
    EXPECT_EQ(PermissionStatus::DENIED,
              permission_context
                  .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                       insecure_url, insecure_url)
                  .status);
    EXPECT_EQ(PermissionStatus::DENIED,
              permission_context
                  .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                       insecure_url, secure_url)
                  .status);
  }
}

TEST_F(WakeLockPermissionContextTests, TestScreenLockPermissionRequest) {
  WakeLockPermissionContext permission_context(
      browser_context(), ContentSettingsType::WAKE_LOCK_SCREEN);
  GURL url("https://www.example.com");
  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .status);
}

TEST_F(WakeLockPermissionContextTests, TestSystemLockPermissionRequest) {
  WakeLockPermissionContext permission_context(
      browser_context(), ContentSettingsType::WAKE_LOCK_SYSTEM);
  GURL url("https://www.example.com");
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .status);
}

}  // namespace permissions
