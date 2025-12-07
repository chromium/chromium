// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

constexpr char kTestingProfilePath[] = "/network/profile/0";
constexpr char kTestSsid[] = "Test SSID";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

class ConfigureNetworkInteractiveUITest : public InteractiveAshTest {
 public:
  std::optional<bool> ServiceIsShared(const std::string& ssid) {
    const base::Value::Dict* properties = GetServicePropertiesUsingName(ssid);
    if (!properties) {
      return std::nullopt;
    }
    const std::string* profile =
        properties->FindString(shill::kProfileProperty);
    return profile && *profile == NetworkProfileHandler::GetSharedProfilePath();
  }

 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();

    // We need a profile to exist in order to configure non-shared networks.
    ShillProfileClient::TestInterface* profile_test =
        ShillProfileClient::Get()->GetTestInterface();
    CHECK(profile_test);
    profile_test->AddProfile(kTestingProfilePath,
                             TestingProfile::kTestUserProfileDir);
  }

 private:
  const base::Value::Dict* GetServicePropertiesUsingName(
      const std::string& name) {
    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    const std::string service_path =
        service_test->FindServiceMatchingName(kTestSsid);
    if (service_path.empty()) {
      return nullptr;
    }
    return service_test->GetServiceProperties(service_path);
  }
};

IN_PROC_BROWSER_TEST_F(ConfigureNetworkInteractiveUITest,
                       NetworkCanBeSharedAndNotShared) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingStateObserver<std::optional<bool>>,
      kShillServiceExistsWithProperties);

  InteractiveAshTest::WifiDialogConfig config;
  config.ssid = kTestSsid;

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Opening the \"Add Wi-Fi\" dialog in OS Settings"),

      OpenAddWifiDialog(kOSSettingsId),

      Log("Filling out the \"Add Wi-Fi\" dialog"),

      CompleteAddWifiDialog(kOSSettingsId, config),

      Log("Checking that the service is shared"),

      PollState(kShillServiceExistsWithProperties,
                base::BindRepeating(
                    &ConfigureNetworkInteractiveUITest::ServiceIsShared,
                    base::Unretained(this), config.ssid)),
      WaitForState(kShillServiceExistsWithProperties, true));

  // Remove the configured service.
  ShillServiceClient::Get()->GetTestInterface()->ClearServices();

  // Update the configuration so that the next Wi-Fi network will not be
  // shared.
  config.is_shared = false;

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Opening the \"Add Wi-Fi\" dialog in OS Settings again"),

      OpenAddWifiDialog(kOSSettingsId),

      Log("Filling out the \"Add Wi-Fi\" dialog again"),

      CompleteAddWifiDialog(kOSSettingsId, config),

      Log("Checking that the service is not shared"),

      WaitForState(kShillServiceExistsWithProperties, false),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
