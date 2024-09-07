// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/hotspot/hotspot_state_observer.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(HotspotStateObserver, kHotspotStateService);

class HotspotPolicyInteractiveUITest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();

    ShillManagerClient::Get()
        ->GetTestInterface()
        ->SetSimulateCheckTetheringReadinessResult(
            FakeShillSimulatedResult::kSuccess,
            shill::kTetheringReadinessReady);

    ShillServiceClient::Get()->GetTestInterface()->AddService(
        shill_service_info_.service_path(), shill_service_info_.service_guid(),
        shill_service_info_.service_name(), shill::kTypeCellular,
        shill::kStateOnline, /*visible=*/true);
  }

  void RestrictHotspotFromPolicy() {
    base::Value::Dict global_config;
    global_config.Set(::onc::global_network_config::kAllowCellularHotspot,
                      false);
    NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, std::string(), base::Value::List(),
        global_config);
  }

 private:
  const ShillServiceInfo shill_service_info_ =
      ShillServiceInfo(/*id=*/0, shill::kTypeCellular);
};

IN_PROC_BROWSER_TEST_F(HotspotPolicyInteractiveUITest,
                       ApplyPolicyWithHotspotOff) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item to exist"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),

      Log("Enforce the policy to restrict users from enabling hotspot"),

      Do([&]() { RestrictHotspotFromPolicy(); }),

      Log("Wait for hotspot toggle to be disabled"),

      WaitForElementDisabled(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Wait for the policy icon to be shown"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotPolicyIcon()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(HotspotPolicyInteractiveUITest,
                       ApplyPolicyWithHotspotOn) {
  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateTetheringEnableResult(FakeShillSimulatedResult::kSuccess,
                                         shill::kTetheringEnableResultSuccess);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item to exist"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),

      WaitForElementEnabled(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Make sure hotspot is initially disabled"),

      ObserveState(kHotspotStateService,
                   std::make_unique<HotspotStateObserver>()),
      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kDisabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         false),

      Log("Waiting for hotspot toggle to be enabled then click it"),

      ClickElement(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Wait for the hotspot state to be enabled"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kEnabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         true),

      Log("Enforce the policy to restrict users from enabling hotspot"),

      Do([&]() { RestrictHotspotFromPolicy(); }),

      Log("Wait for hotspot toggle to be turned off"),

      WaitForElementDisabled(kOSSettingsId, settings::hotspot::HotspotToggle()),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         false),

      Log("Wait for the policy icon to be shown"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotPolicyIcon()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
