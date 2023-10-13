// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

constexpr char kPrivacyAndSecuritySectionPath[] = "osPrivacy";
constexpr char kPrivacyHubSubpagePath[] = "osPrivacy/privacyHub";

// Base class for privacy hub tests in this file.
class PrivacyHubInteractiveUiTest : public InteractiveAshTest {
 public:
  // Shows OS Settings and loads a sub-page.
  auto ShowOSSettingsSubPage(const std::string& sub_page) {
    return Do([&]() {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          GetActiveUserProfile(), sub_page);
    });
  }

  // Returns a query to pierce through Shadow DOM to find the camera toggle.
  static DeepQuery GetCameraToggleQuery() {
    return {"os-settings-ui",
            "os-settings-main",
            "main-page-container",
            "os-settings-privacy-page",
            "settings-privacy-hub-subpage",
            "settings-toggle-button#cameraToggle"};
  }

  // Returns a query to find the microphone toggle.
  static DeepQuery GetMicrophoneToggleQuery() {
    return {
        "os-settings-ui",
        "os-settings-main",
        "main-page-container",
        "os-settings-privacy-page",
        "settings-privacy-hub-subpage",
        "settings-toggle-button#microphoneToggle",
    };
  }

  // Returns a query to find the location toggle.
  static DeepQuery GetGeolocationToggleQuery() {
    return {
        "os-settings-ui",
        "os-settings-main",
        "main-page-container",
        "os-settings-privacy-page",
        "settings-privacy-hub-subpage",
        "settings-toggle-button#geolocationToggle",
    };
  }

  // Returns a query to find the privacy controls subpage trigger.
  static DeepQuery GetPrivacyControlsSubpageTrigger() {
    return {
        "os-settings-ui",
        "os-settings-main",
        "main-page-container",
        "os-settings-privacy-page",
        "cr-link-row#privacyHubSubpageTrigger",
    };
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();
  }
};

// Tests for "V1" privacy hub, which has a geolocation toggle.
class PrivacyHubV1InteractiveUiTest : public PrivacyHubInteractiveUiTest {
 public:
  PrivacyHubV1InteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCrosPrivacyHub,
                              features::kCrosPrivacyHubV0},
        /*disabled_features=*/{});
    CHECK(features::IsCrosPrivacyHubEnabled());
    CHECK(features::IsCrosPrivacyHubLocationEnabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacyHubV1InteractiveUiTest, SettingsPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(kPrivacyHubSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub page to load"),
      WaitForWebContentsReady(kOsSettingsWebContentsId,
                              chrome::GetOSSettingsUrl(kPrivacyHubSubpagePath)),

      Log("Waiting for camera toggle to exist"),
      WaitForElementExists(kOsSettingsWebContentsId, GetCameraToggleQuery()),

      Log("Waiting for microphone toggle to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetMicrophoneToggleQuery()),

      Log("Waiting for geolocation toggle to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetGeolocationToggleQuery()),

      Log("Test complete"));
}

// Tests for "V0" privacy hub, which does not have a geolocation toggle.
class PrivacyHubV0InteractiveUiTest : public PrivacyHubInteractiveUiTest {
 public:
  PrivacyHubV0InteractiveUiTest() {
    feature_list_.InitAndEnableFeature(features::kCrosPrivacyHubV0);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacyHubV0InteractiveUiTest, SettingsPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(kPrivacyHubSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub page to load"),
      WaitForWebContentsReady(kOsSettingsWebContentsId,
                              chrome::GetOSSettingsUrl(kPrivacyHubSubpagePath)),

      Log("Waiting for camera toggle to exist"),
      WaitForElementExists(kOsSettingsWebContentsId, GetCameraToggleQuery()),

      Log("Waiting for microphone toggle to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetMicrophoneToggleQuery()),

      Log("Test complete"));
}

// Tests for privacy hub disabled.
class PrivacyHubDisabledInteractiveUiTest : public PrivacyHubInteractiveUiTest {
 public:
  PrivacyHubDisabledInteractiveUiTest() {
    // Privacy hub can be enabled by multiple feature flags, which can be true
    // in the field trial config JSON file. Ensure all features are disabled.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, /*disabled_features=*/{
            features::kCrosPrivacyHub, features::kCrosPrivacyHubV0,
            features::kVideoConference});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacyHubDisabledInteractiveUiTest, SettingsPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(kPrivacyAndSecuritySectionPath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy section to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(kPrivacyAndSecuritySectionPath)),

      Log("Verifying that privacy controls subpage trigger does not exist"),
      WaitForElementDoesNotExist(kOsSettingsWebContentsId,
                                 GetPrivacyControlsSubpageTrigger()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
