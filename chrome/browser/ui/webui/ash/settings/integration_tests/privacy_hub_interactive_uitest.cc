// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

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

  // Returns a query to pierce through Shadow DOM to find the camera settings
  // toggle button.
  static DeepQuery GetCameraSettingsToggleButtonQuery() {
    return {"os-settings-ui",
            "os-settings-main",
            "main-page-container",
            "os-settings-privacy-page",
            "settings-privacy-hub-subpage",
            "settings-toggle-button#cameraToggle"};
  }

  // Returns a query to find the microphone settings toggle button.
  static DeepQuery GetMicrophoneSettingsToggleButtonQuery() {
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
        "cr-link-row#geolocationAreaLinkRow",
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

// Tests for privacy hub app permissions feature.
class PrivacyHubAppPermissionsInteractiveUiTest
    : public PrivacyHubInteractiveUiTest {
 public:
  PrivacyHubAppPermissionsInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCrosPrivacyHubV0,
                              features::kCrosPrivacyHubAppPermissions},
        /*disabled_features=*/{});
    CHECK(features::IsCrosPrivacyHubV0Enabled());
    CHECK(features::IsCrosPrivacyHubAppPermissionsEnabled());
  }

  // Privacy hub app permissions features replaces the camera settings toggle
  // button in the Privacy hub subpage with a camera subpage trigger followed by
  // a toggle button.
  // Returns a query to find the camera subpage trigger.
  static DeepQuery GetCameraSubpageTriggerQuery() {
    return {
        "os-settings-ui",
        "os-settings-main",
        "main-page-container",
        "os-settings-privacy-page",
        "settings-privacy-hub-subpage",
        "cr-link-row#cameraSubpageLink",
    };
  }

  // Returns a query to find the camera toggle button.
  static DeepQuery GetCameraToggleButtonQuery() {
    return {"os-settings-ui",
            "os-settings-main",
            "main-page-container",
            "os-settings-privacy-page",
            "settings-privacy-hub-subpage",
            "cr-toggle#cameraToggle"};
  }

  // Returns a query to find the microphone subpage trigger.
  static DeepQuery GetMicrophoneSubpageTriggerQuery() {
    return {
        "os-settings-ui",
        "os-settings-main",
        "main-page-container",
        "os-settings-privacy-page",
        "settings-privacy-hub-subpage",
        "cr-link-row#microphoneSubpageLink",
    };
  }

  // Returns a query to find the microphone toggle button.
  static DeepQuery GetMicrophoneToggleButtonQuery() {
    return {"os-settings-ui",
            "os-settings-main",
            "main-page-container",
            "os-settings-privacy-page",
            "settings-privacy-hub-subpage",
            "cr-toggle#microphoneToggle"};
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacyHubAppPermissionsInteractiveUiTest,
                       PrivacyHubSubpage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(chromeos::settings::mojom::kPrivacyHubSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub page to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubSubpagePath)),

      Log("Waiting for camera subpage trigger to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetCameraSubpageTriggerQuery()),

      Log("Waiting for camera toggle button to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetCameraToggleButtonQuery()),

      Log("Waiting for microphone subpage trigger to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetMicrophoneSubpageTriggerQuery()),

      Log("Waiting for microphone toggle button to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetMicrophoneToggleButtonQuery()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(PrivacyHubAppPermissionsInteractiveUiTest,
                       CameraSubpage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(
          chromeos::settings::mojom::kPrivacyHubCameraSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub camera subpage to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubCameraSubpagePath)),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(PrivacyHubAppPermissionsInteractiveUiTest,
                       MicrophoneSubpage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(
          chromeos::settings::mojom::kPrivacyHubMicrophoneSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub microphone subpage to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubMicrophoneSubpagePath)),

      Log("Test complete"));
}

// Tests for "V1" privacy hub, which has a geolocation toggle.
class PrivacyHubV1InteractiveUiTest : public PrivacyHubInteractiveUiTest {
 public:
  PrivacyHubV1InteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCrosPrivacyHub,
                              features::kCrosPrivacyHubV0},
        /*disabled_features=*/{features::kCrosPrivacyHubAppPermissions});
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
      ShowOSSettingsSubPage(chromeos::settings::mojom::kPrivacyHubSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub page to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubSubpagePath)),

      Log("Waiting for camera settings toggle button to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetCameraSettingsToggleButtonQuery()),

      Log("Waiting for microphone settings toggle button to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetMicrophoneSettingsToggleButtonQuery()),

      Log("Waiting for geolocation toggle to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetGeolocationToggleQuery()),

      Log("Test complete"));
}

// Tests for "V0" privacy hub, which does not have a geolocation toggle.
class PrivacyHubV0InteractiveUiTest : public PrivacyHubInteractiveUiTest {
 public:
  PrivacyHubV0InteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCrosPrivacyHubV0},
        /*disabled_features=*/{features::kCrosPrivacyHubAppPermissions});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacyHubV0InteractiveUiTest, SettingsPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(chromeos::settings::mojom::kPrivacyHubSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub page to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubSubpagePath)),

      Log("Waiting for camera settings toggle button to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetCameraSettingsToggleButtonQuery()),

      Log("Waiting for microphone settings toggle button to exist"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           GetMicrophoneSettingsToggleButtonQuery()),

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
      ShowOSSettingsSubPage(
          chromeos::settings::mojom::kPrivacyAndSecuritySectionPath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy section to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyAndSecuritySectionPath)),

      Log("Verifying that privacy controls subpage trigger does not exist"),
      WaitForElementDoesNotExist(kOsSettingsWebContentsId,
                                 GetPrivacyControlsSubpageTrigger()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
