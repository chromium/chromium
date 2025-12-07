// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/ash_element_identifiers.h"
#include "ash/style/switch.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_device_power_state_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/polling_view_observer.h"

namespace ash {
namespace {

class ToggleCellularUiTest : public InteractiveAshTest {
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

IN_PROC_BROWSER_TEST_F(ToggleCellularUiTest,
                       EnableDisableMobileDataFromQuickSettings) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kMobileDataPoweredState);

  // Use a poller because the toggle gets set on a small delay, and we want to
  // avoid race conditions when checking the state.
  using ToggleObserver =
      views::test::PollingViewPropertyObserver<bool, views::ToggleButton>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ToggleObserver, kToggleButtonState);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequence(
      ObserveState(
          kMobileDataPoweredState,
          std::make_unique<ShillDevicePowerStateObserver>(
              ShillManagerClient::Get(), NetworkTypePattern::Mobile())),

      Log("Opening the Quick Settings bubble"),

      OpenQuickSettings(), NavigateQuickSettingsToNetworkPage(),

      Log("Opening the Quick Settings bubble and navigating to the network "
          "page"),

      WaitForShow(ash::kNetworkDetailedViewMobileDataToggleElementId),
      PollViewProperty(kToggleButtonState,
                       ash::kNetworkDetailedViewMobileDataToggleElementId,
                       &views::ToggleButton::GetIsOn),
      WaitForState(kMobileDataPoweredState, true),
      WaitForState(kToggleButtonState, true),

      Log("Disabling mobile data"),

      MoveMouseTo(ash::kNetworkDetailedViewMobileDataToggleElementId),
      ClickMouse(), WaitForState(kMobileDataPoweredState, false),
      WaitForState(kToggleButtonState, false),

      Log("Enabling mobile data"),

      MoveMouseTo(ash::kNetworkDetailedViewMobileDataToggleElementId),
      ClickMouse(), WaitForState(kMobileDataPoweredState, true),
      WaitForState(kToggleButtonState, true),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(ToggleCellularUiTest,
                       EnableDisableMobileDataFromOsSettings) {
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
