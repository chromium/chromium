// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/midi_permission_context.h"

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/features.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

class MidiPermissionContextTests : public testing::Test {
 public:
  void EnableBlockMidiByDefault() {
    feature_list_.InitAndEnableFeature(features::kBlockMidiByDefault);
  }
  content::TestBrowserContext* browser_context() { return &browser_context_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestPermissionsClient client_;
};

// Web MIDI permission status should be allowed only for secure origins.
TEST_F(MidiPermissionContextTests, TestNoSysexAllowedAllOrigins) {
  MidiPermissionContext permission_context(browser_context());
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, insecure_url)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, secure_url)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     secure_url, secure_url)
                .content_setting);
}

// Web MIDI permission status should be denied for insecure origins.
TEST_F(MidiPermissionContextTests, TestInsecureQueryingUrl) {
  EnableBlockMidiByDefault();

  MidiPermissionContext permission_context(browser_context());
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  // Check that there is no saved content settings.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            PermissionsClient::Get()
                ->GetSettingsMap(browser_context())
                ->GetContentSetting(insecure_url, insecure_url,
                                    ContentSettingsType::MIDI));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            PermissionsClient::Get()
                ->GetSettingsMap(browser_context())
                ->GetContentSetting(secure_url, insecure_url,
                                    ContentSettingsType::MIDI));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            PermissionsClient::Get()
                ->GetSettingsMap(browser_context())
                ->GetContentSetting(insecure_url, secure_url,
                                    ContentSettingsType::MIDI));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, insecure_url)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, secure_url)
                .content_setting);
}

}  // namespace permissions
