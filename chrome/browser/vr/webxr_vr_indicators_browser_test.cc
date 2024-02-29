// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

// Browser tests for indicators shown at various phases of an immersive session.

namespace vr {

namespace {

struct TestIndicatorSetting {
  TestIndicatorSetting(ContentSettingsType setting_type,
                       ContentSetting setting,
                       UserFriendlyElementName name,
                       bool visibility)
      : content_setting_type(setting_type),
        content_setting(setting),
        element_name(name),
        element_visibility(visibility) {}
  ContentSettingsType content_setting_type;
  ContentSetting content_setting;
  UserFriendlyElementName element_name;
  bool element_visibility;
};

struct TestContentSettings {
  TestContentSettings(ContentSettingsType setting_type, ContentSetting setting)
      : content_setting_type(setting_type), content_setting(setting) {}
  ContentSettingsType content_setting_type;
  ContentSetting content_setting;
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
    host_content_settings_map->SetContentSettingDefaultScope(
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

// Tests that indicators are displayed in the headset when a device becomes
// in-use.
void TestIndicatorOnAccessForContentType(
    WebXrVrBrowserTestBase* t,
    ContentSettingsType content_setting_type,
    const std::string& script,
    UserFriendlyElementName element_name) {
  // Enter VR while the content setting is CONTENT_SETTING_ASK to suppress
  // its corresponding indicator from initially showing up.
  LoadGenericPageChangeDefaultPermissionAndEnterVr(
      t, {{content_setting_type, CONTENT_SETTING_ASK}});

  // Now, change the setting to allow so the in-use indicator shows up on device
  // usage.
  SetMultipleContentSetting(t, {{content_setting_type, CONTENT_SETTING_ALLOW}});
  t->RunJavaScriptOrFail(script);

  auto utils = UiUtils::Create();
  // Check if the location indicator shows.
  utils->WaitForVisibilityStatus(element_name, true);

  t->EndSessionOrFail();
}

// Tests indicators on entering immersive session.
void TestForInitialIndicatorForContentType(
    WebXrVrBrowserTestBase* t,
    const std::vector<TestIndicatorSetting>& test_indicator_settings) {
  DCHECK(!test_indicator_settings.empty());
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

}  // namespace

// Tests for indicators when they become in-use
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestLocationInUseIndicator) {
  // Asking for location seems to work without any hardware/machine specific
  // enabling/capability (unlike microphone, camera). Hence, this test.
  TestIndicatorOnAccessForContentType(
      t, ContentSettingsType::GEOLOCATION,
      "navigator.geolocation.getCurrentPosition( ()=>{}, ()=>{} )",
      UserFriendlyElementName::kWebXrLocationPermissionIndicator);
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(DISABLED_TestMicrophoneInUseIndicator) {
  TestIndicatorOnAccessForContentType(
      t, ContentSettingsType::MEDIASTREAM_MIC,
      "navigator.getUserMedia( {audio : true},  ()=>{}, ()=>{} )",
      UserFriendlyElementName::kWebXrAudioIndicator);
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(DISABLED_TestCameraInUseIndicator) {
  TestIndicatorOnAccessForContentType(
      t, ContentSettingsType::MEDIASTREAM_CAMERA,
      "navigator.getUserMedia( {video : true},  ()=>{}, ()=>{} )",
      UserFriendlyElementName::kWebXrVideoPermissionIndicator);
}

// Single indicator tests on entering immersive session
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestLocationIndicatorWhenPermissionInitiallyAllowed) {
  TestForInitialIndicatorForContentType(
      t, {{ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW,
           UserFriendlyElementName::kWebXrLocationPermissionIndicator, true}});
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestLocationIndicatorWhenPermissionInitiallyBlocked) {
  TestForInitialIndicatorForContentType(
      t, {{ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK,
           UserFriendlyElementName::kWebXrLocationPermissionIndicator, false}});
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestLocationIndicatorWhenUserAskedToPrompt) {
  TestForInitialIndicatorForContentType(
      t, {{ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK,
           UserFriendlyElementName::kWebXrLocationPermissionIndicator, false}});
}

// Indicator combination tests on entering immersive session
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestMultipleInitialIndicators_NoDevicesAllowed) {
  TestForInitialIndicatorForContentType(
      t,
      {
          {ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK,
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
          {ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK,
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
             {ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW,
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
             {ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW,
              UserFriendlyElementName::kWebXrLocationPermissionIndicator, true},
             {ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW,
              UserFriendlyElementName::kWebXrAudioIndicator, true},
             {ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW,
              UserFriendlyElementName::kWebXrVideoPermissionIndicator, true},
         });
}

}  // namespace vr
