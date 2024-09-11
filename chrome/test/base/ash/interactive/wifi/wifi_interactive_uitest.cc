// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/style/rounded_container.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_device_power_state_observer.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/interaction/polling_view_observer.h"

namespace ash {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

class WifiInteractiveUiTest : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();
  }

  void ConfigureWifi(bool connected) {
    wifi_service_info_.ConfigureService(connected);
  }

  const std::string WifiServicePath() const {
    return wifi_service_info_.service_path();
  }

  const std::string WifiServiceName() const {
    return wifi_service_info_.service_name();
  }

 private:
  const ShillServiceInfo wifi_service_info_{/*id=*/0, shill::kTypeWifi};
};

IN_PROC_BROWSER_TEST_F(WifiInteractiveUiTest, EnableDisableFromOsSettings) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kWifiPoweredState);

  ConfigureWifi(/*connected=*/true);

  // Ensure the OS Settings app is installed.
  InstallSystemApps();

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      ObserveState(kWifiPoweredState,
                   std::make_unique<ShillDevicePowerStateObserver>(
                       ShillManagerClient::Get(), NetworkTypePattern::WiFi())),
      WaitForState(kWifiPoweredState, true),

      Log("Navigate to the WiFi subpage"),

      NavigateSettingsToNetworkSubpage(kOSSettingsId,
                                       NetworkTypePattern::WiFi()),
      WaitForElementTextContains(
          kOSSettingsId, settings::InternetSettingsSubpageTitle(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_TYPE_WIFI)),
      WaitForElementExists(kOSSettingsId, settings::wifi::WifiNetworksList()),
      WaitForToggleState(kOSSettingsId,
                         settings::wifi::WifiSubpageEnableToggle(),
                         /*is_checked=*/true),

      Log("Disable WiFi from WiFi subpage"),

      ClickElement(kOSSettingsId, settings::wifi::WifiSubpageEnableToggle()),
      WaitForToggleState(kOSSettingsId,
                         settings::wifi::WifiSubpageEnableToggle(), false),
      WaitForState(kWifiPoweredState, false),
      WaitForElementDisplayNone(kOSSettingsId,
                                settings::wifi::WifiNetworksList()),

      Log("Enable WiFi from WiFi subpage"),

      ClickElement(kOSSettingsId, settings::wifi::WifiSubpageEnableToggle()),
      WaitForToggleState(kOSSettingsId,
                         settings::wifi::WifiSubpageEnableToggle(), true),
      WaitForState(kWifiPoweredState, true),
      WaitForElementExists(kOSSettingsId, settings::wifi::WifiNetworksList()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(WifiInteractiveUiTest, EnableDisableFromQuickSettings) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kWifiPoweredState);

  ConfigureWifi(/*connected=*/true);

  // Use a poller because the toggle gets set on a small delay, and we want to
  // avoid race conditions when checking the state.
  using ToggleObserver =
      views::test::PollingViewPropertyObserver<bool, views::ToggleButton>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ToggleObserver, kToggleButtonState);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequence(
      ObserveState(kWifiPoweredState,
                   std::make_unique<ShillDevicePowerStateObserver>(
                       ShillManagerClient::Get(), NetworkTypePattern::WiFi())),

      Log("Opening the Quick Settings bubble and navigating to the network "
          "page"),

      OpenQuickSettings(), NavigateQuickSettingsToNetworkPage(),

      Log("Waiting for the network page to be shown and WiFi to have the "
          "expected state"),

      WaitForShow(kNetworkDetailedViewWifiToggleElementId),
      PollViewProperty(kToggleButtonState,
                       kNetworkDetailedViewWifiToggleElementId,
                       &views::ToggleButton::GetIsOn),
      WaitForState(kWifiPoweredState, true),
      WaitForState(kToggleButtonState, true),
      WaitForShow(kNetworkDetailedViewWifiNetworkListElementId),

      Log("Disable WiFi from Quick settings"),

      MoveMouseTo(kNetworkDetailedViewWifiToggleElementId), ClickMouse(),
      WaitForState(kWifiPoweredState, false),
      WaitForState(kToggleButtonState, false),
      WaitForHide(kNetworkDetailedViewWifiNetworkListElementId),

      Log("Enable WiFi from Quick settings"),

      MoveMouseTo(kNetworkDetailedViewWifiToggleElementId), ClickMouse(),
      WaitForState(kWifiPoweredState, true),
      WaitForState(kToggleButtonState, true),
      WaitForShow(kNetworkDetailedViewWifiNetworkListElementId),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(WifiInteractiveUiTest, ConnectFromSettingsSubpage) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kIsWifiConnected);
  ConfigureWifi(/*connected=*/false);

  // Ensure the OS Settings app is installed.
  InstallSystemApps();

  const WebContentsInteractionTestUtil::DeepQuery kWifiNetworkItem(
      {"network-list-item"});
  const WebContentsInteractionTestUtil::DeepQuery kWifiItemTitle =
      kWifiNetworkItem + "div#itemTitle";

  const WebContentsInteractionTestUtil::DeepQuery kWifiItemSublabel =
      kWifiNetworkItem + "div#sublabel";

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      PollState(kIsWifiConnected,
                [this]() -> bool {
                  const auto* shill_service_client_test =
                      ShillServiceClient::Get()->GetTestInterface();
                  CHECK(shill_service_client_test);
                  const auto* wifi_properties =
                      shill_service_client_test->GetServiceProperties(
                          WifiServicePath());
                  CHECK(wifi_properties);
                  const std::string* connected =
                      wifi_properties->FindString(shill::kStateProperty);
                  return connected && *connected == shill::kStateOnline;
                }),
      WaitForState(kIsWifiConnected, false),

      Log("Navigate to the WiFi subpage"),

      NavigateSettingsToNetworkSubpage(kOSSettingsId,
                                       NetworkTypePattern::WiFi()),
      WaitForElementTextContains(
          kOSSettingsId, settings::InternetSettingsSubpageTitle(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_TYPE_WIFI)),
      WaitForElementExists(kOSSettingsId, settings::wifi::WifiNetworksList()),

      Log("Connect to a Wifi network"),

      ClickAnyElementTextContains(kOSSettingsId,
                                  settings::wifi::WifiNetworksList(),
                                  kWifiItemTitle, WifiServiceName()),
      WaitForState(kIsWifiConnected, true),
      WaitForAnyElementAndSiblingTextContains(
          kOSSettingsId, settings::wifi::WifiNetworksList(), kWifiNetworkItem,
          kWifiItemTitle, WifiServiceName(), kWifiItemSublabel,
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),
      Log("Test complete"));
}

}  // namespace
}  // namespace ash
