// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/hotspot/hotspot_config_observer.h"
#include "chrome/test/base/ash/interactive/hotspot/hotspot_state_observer.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

const char kTestSSID[] = "testssid";
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(HotspotStateObserver, kHotspotStateService);

class HotspotUpdateConfigurationInteractiveUITest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();

    ShillServiceClient::Get()->GetTestInterface()->AddService(
        shill_service_info().service_path(),
        shill_service_info().service_guid(),
        shill_service_info().service_name(), shill::kTypeCellular,
        shill::kStateOnline, /*visible=*/true);

    auto* shill_manager_client = ShillManagerClient::Get()->GetTestInterface();
    shill_manager_client->SetSimulateCheckTetheringReadinessResult(
        FakeShillSimulatedResult::kSuccess, shill::kTetheringReadinessReady);
  }

  const ShillServiceInfo& shill_service_info() { return shill_service_info_; }

 private:
  const ShillServiceInfo shill_service_info_ =
      ShillServiceInfo(/*id=*/0, shill::kTypeCellular);
};

IN_PROC_BROWSER_TEST_F(HotspotUpdateConfigurationInteractiveUITest,
                       UpdateHotspotConfiguration) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item to exist then click it"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotSummaryItem()),

      Log("Wait for the configure button to exist and then click it"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotConfigureButton()),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotConfigureButton()),

      Log("Enter the new hotspot SSID and then save it"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSSIDInput()),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotSSIDInput()),
      SendTextAsKeyEvents(kOSSettingsId, kTestSSID),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotDialogSaveButton()),

      Log("Wait for the new hotspot SSID to show up"),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::hotspot::HotspotSSID(),
                                 /*text=*/kTestSSID),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(HotspotUpdateConfigurationInteractiveUITest,
                       UpdateHotspotConfigurationWithHotspotOn) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  ShillManagerClient::Get()
      ->GetTestInterface()
      ->SetSimulateTetheringEnableResult(FakeShillSimulatedResult::kSuccess,
                                         shill::kTetheringEnableResultSuccess);

  ShillManagerClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::Seconds(2));

  HotspotConfigObserver observer;

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for hotspot summary item to exist"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),

      Log("Wait for hotspot toggle to be enabled then click it"),

      WaitForElementEnabled(kOSSettingsId, settings::hotspot::HotspotToggle()),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotToggle()),

      Log("Wait for the hotspot state to be enabled"),

      ObserveState(kHotspotStateService,
                   std::make_unique<HotspotStateObserver>()),
      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kEnabled),
      WaitForToggleState(kOSSettingsId, settings::hotspot::HotspotToggle(),
                         true),

      Log("Wait for hotspot summary item to exist then click it"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotSummaryItem()),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotSummaryItem()),

      Log("Wait for the configure button to exist and then click it"),

      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotConfigureButton()),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotConfigureButton()),

      Log("Enter the new hotspot SSID and then save it"),

      ClearInputAndEnterText(kOSSettingsId,
                             settings::hotspot::HotspotSSIDInput(), kTestSSID),
      ClickElement(kOSSettingsId, settings::hotspot::HotspotDialogSaveButton()),

      Log("Wait for configure window to clear"),

      WaitForElementDoesNotExist(kOSSettingsId,
                                 settings::hotspot::HotspotSSIDInput()),

      Log("Wait for hotspot to be disabled"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kDisabled),

      Log("Wait for hotspot to be enabled"),

      WaitForState(kHotspotStateService,
                   hotspot_config::mojom::HotspotState::kEnabled),
      WaitForElementExists(kOSSettingsId,
                           settings::hotspot::HotspotClientCountItem()),

      Log("Wait for the new hotspot SSID to show up"),

      WaitForElementTextContains(kOSSettingsId,
                                 settings::hotspot::HotspotSSID(),
                                 /*text=*/kTestSSID),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
