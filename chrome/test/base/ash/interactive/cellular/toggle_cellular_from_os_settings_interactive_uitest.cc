// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_device_power_state_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

class ToggleCellularFromOsSettingsUiTest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();
  }
};

IN_PROC_BROWSER_TEST_F(ToggleCellularFromOsSettingsUiTest,
                       EnableDisableMobileData) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kMobileDataPoweredState);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Waiting for Mobile data toggle to exist and be enabled"),

      ObserveState(
          kMobileDataPoweredState,
          std::make_unique<ShillDevicePowerStateObserver>(
              ShillManagerClient::Get(), NetworkTypePattern::Mobile())),
      WaitForState(kMobileDataPoweredState, true),
      WaitForToggleState(kOSSettingsId, settings::cellular::MobileDataToggle(),
                         true),

      Log("Disabling mobile data"),

      ClickElement(kOSSettingsId, settings::cellular::MobileDataToggle()),
      WaitForToggleState(kOSSettingsId, settings::cellular::MobileDataToggle(),
                         false),
      WaitForState(kMobileDataPoweredState, false),

      Log("Enabling mobile data"),

      ClickElement(kOSSettingsId, settings::cellular::MobileDataToggle()),
      WaitForToggleState(kOSSettingsId, settings::cellular::MobileDataToggle(),
                         true),
      WaitForState(kMobileDataPoweredState, true),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
