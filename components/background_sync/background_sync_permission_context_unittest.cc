// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_sync/background_sync_permission_context.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class BackgroundSyncPermissionContextTest
    : public content::RenderViewHostTestHarness {
 public:
  BackgroundSyncPermissionContextTest(
      const BackgroundSyncPermissionContextTest&) = delete;
  BackgroundSyncPermissionContextTest& operator=(
      const BackgroundSyncPermissionContextTest&) = delete;

 protected:
  BackgroundSyncPermissionContextTest() = default;
  ~BackgroundSyncPermissionContextTest() override = default;

  void NavigateAndRequestPermission(
      const GURL& url,
      BackgroundSyncPermissionContext* permission_context) {
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

    base::RunLoop run_loop;

    const permissions::PermissionRequestID id(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        permissions::PermissionRequestID::RequestLocalId());
    permission_context->RequestPermission(
        permissions::PermissionRequestData(permission_context, id,
                                           /*user_gesture=*/false, url),
        base::BindOnce(
            &BackgroundSyncPermissionContextTest::TrackPermissionDecision,
            base::Unretained(this), run_loop.QuitClosure()));

    run_loop.Run();
  }

  void TrackPermissionDecision(base::RepeatingClosure done_closure,
                               ContentSetting content_setting) {
    permission_granted_ = content_setting == CONTENT_SETTING_ALLOW;
    std::move(done_closure).Run();
  }

  bool permission_granted() const { return permission_granted_; }

 protected:
  permissions::TestPermissionsClient client_;

 private:
  bool permission_granted_;
};

// Background sync permission should be allowed by default for a secure origin.
TEST_F(BackgroundSyncPermissionContextTest, TestSecureRequestingUrl) {
  GURL url("https://www.example.com");
  BackgroundSyncPermissionContext permission_context(browser_context());

  NavigateAndRequestPermission(url, &permission_context);

  EXPECT_TRUE(permission_granted());
}

// Background sync permission should be denied for an insecure origin.
TEST_F(BackgroundSyncPermissionContextTest, TestInsecureRequestingUrl) {
  GURL url("http://example.com");
  BackgroundSyncPermissionContext permission_context(browser_context());

  NavigateAndRequestPermission(url, &permission_context);

  EXPECT_FALSE(permission_granted());
}

// Tests that blocking one origin does not affect the others.
TEST_F(BackgroundSyncPermissionContextTest, TestBlockOrigin) {
  GURL url1("https://www.example1.com");
  GURL url2("https://www.example2.com");
  BackgroundSyncPermissionContext permission_context(browser_context());
  permissions::PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetContentSettingDefaultScope(url1, GURL(),
                                      ContentSettingsType::BACKGROUND_SYNC,
                                      CONTENT_SETTING_BLOCK);

  NavigateAndRequestPermission(url1, &permission_context);

  EXPECT_FALSE(permission_granted());

  NavigateAndRequestPermission(url2, &permission_context);

  EXPECT_TRUE(permission_granted());
}
