// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

enum class SensorType { kCamera, kMicrophone, kGeolocation };
enum class ControlElementType { kToggle, kSubpageLink, kLegacyToggle };

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

  static DeepQuery ControlElementQuery(SensorType sensor,
                                       ControlElementType element_type) {
    const DeepQuery query_prefix{
        "os-settings-ui", "os-settings-main", "main-page-container",
        "os-settings-privacy-page", "settings-privacy-hub-subpage"};
    const std::string last_segment = [element_type, sensor]() -> std::string {
      if (SensorType::kGeolocation == sensor) {
        return "cr-link-row#geolocationAreaLinkRow";
      }

      const std::string html_tag = [element_type]() {
        switch (element_type) {
          case ControlElementType::kToggle:
            return "cr-toggle";
          case ControlElementType::kSubpageLink:
            return "cr-link-row";
          case ControlElementType::kLegacyToggle:
            return "settings-toggle-button";
        }
      }();
      const std::string element_id_prefix =
          (sensor == SensorType::kCamera) ? "camera" : "microphone";
      const std::string element_id_suffix =
          (element_type == ControlElementType::kSubpageLink) ? "SubpageLink"
                                                             : "Toggle";

      return html_tag + "#" + element_id_prefix + element_id_suffix;
    }();

    return query_prefix + last_segment;
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

class PrivacyHubSettingsPageTest
    : public PrivacyHubInteractiveUiTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  PrivacyHubSettingsPageTest() {
    using FeatureList = std::vector<base::test::FeatureRef>;
    FeatureList enabled_features;
    FeatureList disabled_features;
    const auto feature_set = [&](bool flag_on) -> FeatureList& {
      return flag_on ? enabled_features : disabled_features;
    };
    feature_set(LocationFlagOn()).push_back(features::kCrosPrivacyHub);
    feature_set(PerAppPermissionFlagOn())
        .push_back(features::kCrosPrivacyHubAppPermissions);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool LocationFlagOn() const { return std::get<0>(GetParam()); }
  bool PerAppPermissionFlagOn() const { return std::get<1>(GetParam()); }

  auto WaitForElement(bool condition,
                      const ui::ElementIdentifier& element_id,
                      const DeepQuery& query) {
    return condition ? WaitForElementExists(element_id, query)
                     : WaitForElementDoesNotExist(element_id, query);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This test checks for the presence of the control elements for all sensors
// (camera, microphone, geolocation). The particular control elements and the
// set of sensors differs based on the feature flags. This test verifies the
// right composition of control elements on the Privacy Controls subpage in
// settings for all combinations of feature flags.
IN_PROC_BROWSER_TEST_P(PrivacyHubSettingsPageTest, PrivacyControls) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  ASSERT_EQ(features::IsCrosPrivacyHubLocationEnabled(), LocationFlagOn());
  ASSERT_EQ(features::IsCrosPrivacyHubAppPermissionsEnabled(),
            PerAppPermissionFlagOn());

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

      Log("Waiting for camera subpage trigger to exist or not"),
      WaitForElement(PerAppPermissionFlagOn(), kOsSettingsWebContentsId,
                     ControlElementQuery(SensorType::kCamera,
                                         ControlElementType::kSubpageLink)),

      Log("Waiting for the camera toggle button to exist or not"),
      WaitForElement(PerAppPermissionFlagOn(), kOsSettingsWebContentsId,
                     ControlElementQuery(SensorType::kCamera,
                                         ControlElementType::kToggle)),

      Log("Waiting for the legacy camera toggle button to exist or not"),
      WaitForElement(!PerAppPermissionFlagOn(), kOsSettingsWebContentsId,
                     ControlElementQuery(SensorType::kCamera,
                                         ControlElementType::kLegacyToggle)),

      Log("Waiting for microphone subpage trigger to exist or not"),
      WaitForElement(PerAppPermissionFlagOn(), kOsSettingsWebContentsId,
                     ControlElementQuery(SensorType::kMicrophone,
                                         ControlElementType::kSubpageLink)),

      Log("Waiting for the microphone toggle button to exist or not"),
      WaitForElement(PerAppPermissionFlagOn(), kOsSettingsWebContentsId,
                     ControlElementQuery(SensorType::kMicrophone,
                                         ControlElementType::kToggle)),

      Log("Waiting for the legacy microphone toggle button to exist or not"),
      WaitForElement(!PerAppPermissionFlagOn(), kOsSettingsWebContentsId,
                     ControlElementQuery(SensorType::kMicrophone,
                                         ControlElementType::kLegacyToggle)),

      Log("Waiting for geolocation subpage trigger to exist or not"),
      WaitForElement(LocationFlagOn(), kOsSettingsWebContentsId,
                     ControlElementQuery(SensorType::kGeolocation,
                                         ControlElementType::kSubpageLink)),

      Log("Test complete"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacyHubSettingsPageTest,
                         testing::Combine(
                             /*location_on=*/testing::Bool(),
                             /*per_app_permissions_on=*/testing::Bool()));

// Tests for privacy hub app permissions feature.
class PrivacyHubAppPermissionsInteractiveUiTest
    : public PrivacyHubInteractiveUiTest {
 public:
  PrivacyHubAppPermissionsInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCrosPrivacyHub,
                              features::kCrosPrivacyHubAppPermissions},
        /*disabled_features=*/{});
    CHECK(features::IsCrosPrivacyHubLocationEnabled());
    CHECK(features::IsCrosPrivacyHubAppPermissionsEnabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

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

IN_PROC_BROWSER_TEST_F(PrivacyHubAppPermissionsInteractiveUiTest,
                       GeolocationSubpage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);

  RunTestSequence(
      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      ShowOSSettingsSubPage(
          chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath),
      WaitForShow(kOsSettingsWebContentsId),

      Log("Waiting for OS settings privacy hub geolocation subpage to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath)),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
