// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/webrtc/media_stream_device_enumerator_impl.h"
#include "content/public/test/test_renderer_host.h"

namespace {

struct TestConfig {
  const ContentSetting first;   // first content setting to be set
  const ContentSetting second;  // second content setting to be set
  const ContentSetting result;  // expected resulting content setting
};

// Waits until a change is observed for a specific content setting type.
class ContentSettingsChangeWaiter : public content_settings::Observer {
 public:
  explicit ContentSettingsChangeWaiter(content::BrowserContext* browser_context,
                                       ContentSettingsType content_type)
      : browser_context_(browser_context), content_type_(content_type) {
    permissions::PermissionsClient::Get()
        ->GetSettingsMap(browser_context_)
        ->AddObserver(this);
  }

  ContentSettingsChangeWaiter(const ContentSettingsChangeWaiter&) = delete;
  ContentSettingsChangeWaiter& operator=(const ContentSettingsChangeWaiter&) =
      delete;

  ~ContentSettingsChangeWaiter() override {
    permissions::PermissionsClient::Get()
        ->GetSettingsMap(browser_context_)
        ->RemoveObserver(this);
  }

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override {
    if (content_type_set.Contains(content_type_))
      Proceed();
  }

  void Wait() { run_loop_.Run(); }

 private:
  void Proceed() { run_loop_.Quit(); }

  raw_ptr<content::BrowserContext> browser_context_;
  ContentSettingsType content_type_;
  base::RunLoop run_loop_;
};

}  // namespace

namespace permissions {

namespace {
class TestCameraPanTiltZoomPermissionContextDelegate
    : public CameraPanTiltZoomPermissionContext::Delegate {
 public:
  explicit TestCameraPanTiltZoomPermissionContextDelegate(
      content::BrowserContext* browser_context) {}

  bool GetPermissionStatusInternal(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      ContentSetting* content_setting_result) override {
    // Do not override GetPermissionStatusInternal logic.
    return false;
  }
};
}  // namespace

class CameraPanTiltZoomPermissionContextTests
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<TestConfig> {
 public:
  CameraPanTiltZoomPermissionContextTests() = default;

  void SetContentSetting(ContentSettingsType content_settings_type,
                         ContentSetting content_setting) {
    GURL url("https://www.example.com");
    HostContentSettingsMap* content_settings =
        permissions::PermissionsClient::Get()->GetSettingsMap(
            browser_context());
    content_settings->SetContentSettingDefaultScope(
        url, GURL(), content_settings_type, content_setting);
  }

  ContentSetting GetContentSetting(ContentSettingsType content_settings_type) {
    GURL url("https://www.example.com");
    HostContentSettingsMap* content_settings =
        permissions::PermissionsClient::Get()->GetSettingsMap(
            browser_context());
    return content_settings->GetContentSetting(url.DeprecatedGetOriginAsURL(),
                                               url.DeprecatedGetOriginAsURL(),
                                               content_settings_type);
  }

  const webrtc::MediaStreamDeviceEnumerator* device_enumerator() const {
    return &device_enumerator_;
  }

 private:
  TestPermissionsClient client_;
  webrtc::MediaStreamDeviceEnumeratorImpl device_enumerator_;
};

class CameraContentSettingTests
    : public CameraPanTiltZoomPermissionContextTests {
 public:
  CameraContentSettingTests() = default;
};

TEST_P(CameraContentSettingTests, TestResetPermissionOnCameraChange) {
  auto delegate =
      std::make_unique<TestCameraPanTiltZoomPermissionContextDelegate>(
          browser_context());
  CameraPanTiltZoomPermissionContext permission_context(
      browser_context(), std::move(delegate), device_enumerator());
  ContentSettingsChangeWaiter waiter(browser_context(),
                                     ContentSettingsType::MEDIASTREAM_CAMERA);

  SetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                    GetParam().first);
  SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA, GetParam().second);

  waiter.Wait();
  EXPECT_EQ(GetParam().result,
            GetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM));
}

INSTANTIATE_TEST_SUITE_P(
    ResetPermissionOnCameraChange,
    CameraContentSettingTests,
    testing::Values(
        // Granted camera PTZ permission is reset if camera is blocked.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_ASK},
        // Granted camera PTZ permission is reset if camera is reset.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Blocked camera PTZ permission is not reset if camera is granted.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_BLOCK},
        // Blocked camera PTZ permission is not reset if camera is blocked.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK}));

class CameraPanTiltZoomContentSettingTests
    : public CameraPanTiltZoomPermissionContextTests {
 public:
  CameraPanTiltZoomContentSettingTests() = default;
};

TEST_P(CameraPanTiltZoomContentSettingTests,
       TestCameraPermissionOnCameraPanTiltZoomChange) {
  auto delegate =
      std::make_unique<TestCameraPanTiltZoomPermissionContextDelegate>(
          browser_context());
  CameraPanTiltZoomPermissionContext permission_context(
      browser_context(), std::move(delegate), device_enumerator());

  // Initialize PTZ to a non-default value to be able to reset it later.
  if (GetParam().second == CONTENT_SETTING_DEFAULT) {
    ContentSettingsChangeWaiter waiter(
        browser_context(), ContentSettingsType::CAMERA_PAN_TILT_ZOOM);
    ContentSetting setting = GetParam().first == CONTENT_SETTING_DEFAULT
                                 ? CONTENT_SETTING_ALLOW
                                 : GetParam().first;

    SetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM, setting);
    waiter.Wait();
  }

  ContentSettingsChangeWaiter waiter(browser_context(),
                                     ContentSettingsType::CAMERA_PAN_TILT_ZOOM);
  SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA, GetParam().first);
  SetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                    GetParam().second);

  waiter.Wait();
  EXPECT_EQ(GetParam().result,
            GetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA));
}

INSTANTIATE_TEST_SUITE_P(
    CameraPermissionOnCameraPanTiltZoomChange,
    CameraPanTiltZoomContentSettingTests,
    testing::Values(
        // Asked camera permission is blocked if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Asked camera permission is granted if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Asked camera permission is unchanged if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Asked camera permission is unchanged if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Allowed camera permission is blocked if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Allowed camera permission is unchanged if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Allowed camera permission is ask if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Allowed camera permission is reset if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Blocked camera permission is unchanged if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Blocked camera permission is allowed if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Blocked camera permission is ask if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Blocked camera permission is reset if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Default camera permission is blocked if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Default camera permission is allowed if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Default camera permission is unchanged if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Default camera permission is ask if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK}));

}  // namespace permissions
