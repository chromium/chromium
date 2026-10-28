// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "chrome/browser/vr/vr_browser_renderer_thread.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_util.h"

// Browser tests for indicators shown at various phases of an immersive session.

namespace vr {

namespace {

struct TestIndicatorSetting {
  TestIndicatorSetting(ContentSettingsType setting_type,
                       PermissionSetting setting,
                       UserFriendlyElementName name,
                       bool visibility)
      : content_setting_type(setting_type),
        content_setting(setting),
        element_name(name),
        element_visibility(visibility) {}
  ContentSettingsType content_setting_type;
  PermissionSetting content_setting;
  UserFriendlyElementName element_name;
  bool element_visibility;
};

struct TestContentSettings {
  TestContentSettings(ContentSettingsType setting_type,
                      PermissionSetting setting)
      : content_setting_type(setting_type), content_setting(setting) {}
  ContentSettingsType content_setting_type;
  PermissionSetting content_setting;
};

// Helpers
std::vector<TestContentSettings> ExtractFrom(
    const std::vector<TestIndicatorSetting>& test_indicator_settings) {
  return base::ToVector(
      test_indicator_settings, [](const TestIndicatorSetting& s) {
        return TestContentSettings{s.content_setting_type, s.content_setting};
      });
}

void SetMultipleContentSetting(
    WebXrVrBrowserTestBase* t,
    const std::vector<TestContentSettings>& test_settings) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(Profile::FromBrowserContext(
          t->GetCurrentWebContents()->GetBrowserContext()));
  for (const TestContentSettings& s : test_settings)
    host_content_settings_map->SetPermissionSettingDefaultScope(
        t->GetCurrentWebContents()->GetLastCommittedURL(),
        t->GetCurrentWebContents()->GetLastCommittedURL(),
        s.content_setting_type, s.content_setting);
}

void LoadGenericPageChangeDefaultPermissionAndEnterVr(
    WebXrVrBrowserTestBase* t,
    const std::vector<TestContentSettings>& test_settings) {
  t->LoadFileAndAwaitInitialization("generic_webxr_page");
  SetMultipleContentSetting(t, test_settings);
  t->EnterSessionWithUserGestureOrFail();
}

PermissionSetting CreateGeolocationSetting(ContentSetting setting) {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission)) {
    return GeolocationSetting{content_settings::ToPermissionOption(setting),
                              content_settings::ToPermissionOption(setting)};
  } else {
    return setting;
  }
}

// Tests that indicators are displayed in the headset when a device becomes
// in-use.
void TestIndicatorOnAccessForContentType(
    WebXrVrBrowserTestBase* t,
    ContentSettingsType content_setting_type,
    UserFriendlyElementName element_name) {
  MockXRDeviceHookBase mock_device;
  // Enter VR while the content setting is CONTENT_SETTING_ASK to suppress
  // its corresponding indicator from initially showing up.
  LoadGenericPageChangeDefaultPermissionAndEnterVr(
      t,
      {{content_setting_type, CreateGeolocationSetting(CONTENT_SETTING_ASK)}});

  // Now, change the setting to allow so the in-use indicator shows up on device
  // usage.
  SetMultipleContentSetting(
      t, {{content_setting_type,
           CreateGeolocationSetting(CONTENT_SETTING_ALLOW)}});

  auto utils = UiUtils::Create();

  content_settings::PageSpecificContentSettings::GetForFrame(
      t->GetCurrentWebContents()->GetPrimaryMainFrame())
      ->OnContentAllowed(content_setting_type);

  // Check if the location indicator shows. Use a longer timeout on slower
  // environments (e.g. emulators) because VRUiHostImpl polls capturing state
  // on a timer before posting state updates to the VR rendering thread.
  utils->WaitForVisibilityStatus(element_name, true,
                                 XrBrowserTestBase::kPollTimeoutLong);

  t->EndSessionOrFail();
}

// Tests indicators on entering immersive session.
void TestForInitialIndicatorForContentType(
    WebXrVrBrowserTestBase* t,
    const std::vector<TestIndicatorSetting>& test_indicator_settings) {
  DCHECK(!test_indicator_settings.empty());
  MockXRDeviceHookBase mock_device;
  // Enter VR while the content setting is CONTENT_SETTING_ASK to suppress
  // its corresponding indicator from initially showing up.
  LoadGenericPageChangeDefaultPermissionAndEnterVr(
      t, ExtractFrom(test_indicator_settings));

  auto utils = UiUtils::Create();
  // Check if the location indicator shows.
  for (const TestIndicatorSetting& setting : test_indicator_settings)
    utils->WaitForVisibilityStatus(setting.element_name,
                                   setting.element_visibility);

  t->EndSessionOrFail();
}

TestIndicatorSetting CreateGeolocationSetting(ContentSetting setting,
                                              bool visible) {
  return TestIndicatorSetting(
      permissions::PermissionUtil::GetGeolocationType(),
      CreateGeolocationSetting(setting),
      UserFriendlyElementName::kWebXrLocationPermissionIndicator, visible);
}

}  // namespace

// Tests for indicators when they become in-use
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestLocationInUseIndicator) {
  // Asking for location seems to work without any hardware/machine specific
  // enabling/capability (unlike microphone, camera). Hence, this test.
  TestIndicatorOnAccessForContentType(
      t, permissions::PermissionUtil::GetGeolocationType(),
      UserFriendlyElementName::kWebXrLocationPermissionIndicator);
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(DISABLED_TestMicrophoneInUseIndicator) {
  TestIndicatorOnAccessForContentType(
      t, ContentSettingsType::MEDIASTREAM_MIC,
      UserFriendlyElementName::kWebXrAudioIndicator);
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(DISABLED_TestCameraInUseIndicator) {
  TestIndicatorOnAccessForContentType(
      t, ContentSettingsType::MEDIASTREAM_CAMERA,
      UserFriendlyElementName::kWebXrVideoPermissionIndicator);
}

// Showing a permission prompt while the indicators are visible stops the
// overlay and starts it again right away (see
// VRUiHostImpl::ShowExternalNotificationPrompt()). The restarted overlay
// creates its graphics asynchronously, and an overlay pose or frame submission
// from before the restart may still be in flight; it must not draw until the
// graphics are ready.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPromptWhileIndicatorsVisible) {
  MockXRDeviceHookBase mock_device;
  LoadGenericPageChangeDefaultPermissionAndEnterVr(
      t, {{permissions::PermissionUtil::GetGeolocationType(),
           CreateGeolocationSetting(CONTENT_SETTING_ALLOW)}});

  auto utils = UiUtils::Create();
  utils->WaitForVisibilityStatus(
      UserFriendlyElementName::kWebXrLocationPermissionIndicator, true,
      XrBrowserTestBase::kPollTimeoutLong);
  // The overlay is drawing, so it has a pose request or a submission
  // outstanding.
  mock_device.WaitNumFrames(2);

  VRBrowserRendererThread* renderer_thread =
      VRBrowserRendererThread::GetInstanceForTesting();
  ASSERT_TRUE(renderer_thread);
  renderer_thread->SetIndicatorsVisible(false);
  renderer_thread->SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType::kPromptGenericPermission);

  // Wait for the restarted overlay's graphics, then let it draw.
  utils = UiUtils::Create();
  mock_device.WaitNumFrames(5);
#if !BUILDFLAG(IS_ANDROID)
  // UiSceneCreator only creates the prompt notification on desktop.
  utils->WaitForVisibilityStatus(
      UserFriendlyElementName::kWebXrExternalPromptNotification, true);
#endif

  t->EndSessionOrFail();
}

// Single indicator tests on entering immersive session
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestLocationIndicatorWhenPermissionInitiallyAllowed) {
  TestForInitialIndicatorForContentType(
      t, {CreateGeolocationSetting(CONTENT_SETTING_ALLOW, true)});
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestLocationIndicatorWhenPermissionInitiallyBlocked) {
  TestForInitialIndicatorForContentType(
      t, {CreateGeolocationSetting(CONTENT_SETTING_BLOCK, false)});
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestLocationIndicatorWhenUserAskedToPrompt) {
  TestForInitialIndicatorForContentType(
      t, {CreateGeolocationSetting(CONTENT_SETTING_ASK, false)});
}

// Indicator combination tests on entering immersive session
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestMultipleInitialIndicators_NoDevicesAllowed) {
  TestForInitialIndicatorForContentType(
      t,
      {
          {permissions::PermissionUtil::GetGeolocationType(),
           CreateGeolocationSetting(CONTENT_SETTING_ASK),
           UserFriendlyElementName::kWebXrLocationPermissionIndicator, false},
          {ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ASK,
           UserFriendlyElementName::kWebXrAudioIndicator, false},
          {ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK,
           UserFriendlyElementName::kWebXrVideoPermissionIndicator, false},
      });
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestMultipleInitialIndicators_OneDeviceAllowed) {
  TestForInitialIndicatorForContentType(
      t,
      {
          {permissions::PermissionUtil::GetGeolocationType(),
           CreateGeolocationSetting(CONTENT_SETTING_ASK),
           UserFriendlyElementName::kWebXrLocationPermissionIndicator, false},
          {ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW,
           UserFriendlyElementName::kWebXrAudioIndicator, true},
          {ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK,
           UserFriendlyElementName::kWebXrVideoPermissionIndicator, false},
      });
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestMultipleInitialIndicators_TwoDevicesAllowed) {
  TestForInitialIndicatorForContentType(
      t, {
             {permissions::PermissionUtil::GetGeolocationType(),
              CreateGeolocationSetting(CONTENT_SETTING_ALLOW),
              UserFriendlyElementName::kWebXrLocationPermissionIndicator, true},
             {ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_BLOCK,
              UserFriendlyElementName::kWebXrAudioIndicator, false},
             {ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW,
              UserFriendlyElementName::kWebXrVideoPermissionIndicator, true},
         });
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestMultipleInitialIndicators_ThreeDevicesAllowed) {
  TestForInitialIndicatorForContentType(
      t, {
             {permissions::PermissionUtil::GetGeolocationType(),
              CreateGeolocationSetting(CONTENT_SETTING_ALLOW),
              UserFriendlyElementName::kWebXrLocationPermissionIndicator, true},
             {ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW,
              UserFriendlyElementName::kWebXrAudioIndicator, true},
             {ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW,
              UserFriendlyElementName::kWebXrVideoPermissionIndicator, true},
         });
}

}  // namespace vr
