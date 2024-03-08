// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/midi_sysex_permission_context.h"

#include <string>

#include "base/functional/bind.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

class TestPermissionContext : public MidiSysexPermissionContext {
 public:
  explicit TestPermissionContext(content::BrowserContext* browser_context)
      : MidiSysexPermissionContext(browser_context),
        permission_set_(false),
        permission_granted_(false),
        tab_context_updated_(false) {}

  ~TestPermissionContext() override = default;

  bool permission_granted() { return permission_granted_; }

  bool permission_set() { return permission_set_; }

  bool tab_context_updated() { return tab_context_updated_; }

  void TrackPermissionDecision(ContentSetting content_setting) {
    permission_set_ = true;
    permission_granted_ = content_setting == CONTENT_SETTING_ALLOW;
  }

 protected:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool allowed) override {
    tab_context_updated_ = true;
  }

 private:
  bool permission_set_;
  bool permission_granted_;
  bool tab_context_updated_;
};

}  // namespace

class MidiSysexPermissionContextTests
    : public content::RenderViewHostTestHarness {
 protected:
  MidiSysexPermissionContextTests() = default;
  ~MidiSysexPermissionContextTests() override = default;

 private:
  TestPermissionsClient client_;
};

// Web MIDI sysex permission should be denied for insecure origin.
TEST_F(MidiSysexPermissionContextTests, TestInsecureRequestingUrl) {
  TestPermissionContext permission_context(browser_context());
  GURL url("http://www.example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      permissions::PermissionRequestID::RequestLocalId());
  permission_context.RequestPermission(
      PermissionRequestData(&permission_context, id,
                            /*user_gesture=*/true, url),
      base::BindOnce(&TestPermissionContext::TrackPermissionDecision,
                     base::Unretained(&permission_context)));

  EXPECT_TRUE(permission_context.permission_set());
  EXPECT_FALSE(permission_context.permission_granted());
  EXPECT_TRUE(permission_context.tab_context_updated());

  ContentSetting setting =
      PermissionsClient::Get()
          ->GetSettingsMap(browser_context())
          ->GetContentSetting(url.DeprecatedGetOriginAsURL(),
                              url.DeprecatedGetOriginAsURL(),
                              ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_ASK, setting);
}

// Web MIDI sysex permission status should be denied for insecure origin.
TEST_F(MidiSysexPermissionContextTests, TestInsecureQueryingUrl) {
  TestPermissionContext permission_context(browser_context());
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  // Check that there is no saved content settings.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            PermissionsClient::Get()
                ->GetSettingsMap(browser_context())
                ->GetContentSetting(insecure_url.DeprecatedGetOriginAsURL(),
                                    insecure_url.DeprecatedGetOriginAsURL(),
                                    ContentSettingsType::MIDI_SYSEX));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            PermissionsClient::Get()
                ->GetSettingsMap(browser_context())
                ->GetContentSetting(secure_url.DeprecatedGetOriginAsURL(),
                                    insecure_url.DeprecatedGetOriginAsURL(),
                                    ContentSettingsType::MIDI_SYSEX));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            PermissionsClient::Get()
                ->GetSettingsMap(browser_context())
                ->GetContentSetting(insecure_url.DeprecatedGetOriginAsURL(),
                                    secure_url.DeprecatedGetOriginAsURL(),
                                    ContentSettingsType::MIDI_SYSEX));

  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, insecure_url)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, secure_url)
                .status);
}

}  // namespace permissions
