// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
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
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

class WifiInteractiveUiTest : public InteractiveAshTest {
 protected:
  // Use a poller because the toggle gets set on a small delay, and we want to
  // avoid race conditions when checking the state.
  using ToggleObserver =
      views::test::PollingViewPropertyObserver<bool, views::ToggleButton>;

  using NetworkNameObserver =
      views::test::PollingViewObserver<bool, views::View>;

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();
    ash::ShillServiceClient::TestInterface* service_test =
        ash::ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();

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

  auto VerifyWifiState(
      bool enabled,
      const ui::test::StateIdentifier<ShillDevicePowerStateObserver>&
          wifi_power_state_identifier,
      const ui::test::StateIdentifier<ToggleObserver>&
          toggle_button_state_identifier) {
    auto steps = Steps(WaitForState(wifi_power_state_identifier, enabled),
                       WaitForState(toggle_button_state_identifier, enabled));

    if (enabled) {
      AddStep(steps, WaitForShow(kNetworkDetailedViewWifiNetworkListElementId));
    } else {
      AddStep(steps, WaitForHide(kNetworkDetailedViewWifiNetworkListElementId));
    }

    AddStep(
        steps,
        Steps(
            Log("Verify the WiFi toggle tooltip matches the WiFi's current "
                "state"),

            WaitForShow(kNetworkDetailedViewWifiToggleElementId),
            MoveMouseTo(kNetworkDetailedViewWifiToggleElementId),
            CheckViewProperty(
                kNetworkDetailedViewWifiToggleElementId,
                &views::Button::GetTooltipText,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_WIFI,
                    l10n_util::GetStringUTF16(
                        enabled ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                                : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED))),

            Log("Verifying the progress bar matches the current WiFi's current "
                "state")));

    if (enabled) {
      AddStep(steps,
              std::move(WaitForShow(kTrayDetailedViewProgressBarElementId)
                            .SetMustRemainVisible(false)));
    } else {
      AddStep(steps, WaitForHide(kTrayDetailedViewProgressBarElementId));
    }

    return steps;
  }

  auto PollNetworkInList(const std::string& network_name,
                         const ui::test::StateIdentifier<NetworkNameObserver>&
                             polling_identifier) {
    return Steps(PollView(
        polling_identifier, kNetworkDetailedViewWifiNetworkListElementId,
        [&](const views::View* view) -> bool {
          for (auto& child : view->children()) {
            if (views::IsViewClass<NetworkListNetworkItemView>(child)) {
              const auto network_label =
                  views::AsViewClass<NetworkListNetworkItemView>(child)
                      ->text_label()
                      ->GetText();
              if (network_label == base::ASCIIToUTF16(WifiServiceName())) {
                return true;
              }
            }
          }
          return false;
        },
        base::Milliseconds(50)));
  }

 private:
  const ShillServiceInfo wifi_service_info_{/*id=*/0, shill::kTypeWifi};
};

IN_PROC_BROWSER_TEST_F(WifiInteractiveUiTest,
                       ToggleAndCheckOsSettingsWiFiPageElements) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kWifiPoweredState);

  ConfigureWifi(/*connected=*/true);

  // Set this delay so the WiFi scanning progress bar shows.
  ShillManagerClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::Seconds(2));

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
      WaitForElementExists(
          kOSSettingsId, settings::wifi::WiFiSubpageSearchForNetworksSpinner()),
      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::WiFiSubpageSearchForNetworks(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_SCANNING_MESSAGE)),
      WaitForElementDisplayNotNone(kOSSettingsId,
                                   settings::wifi::WiFiSubpageNetworkListDiv()),
      WaitForToggleState(kOSSettingsId,
                         settings::wifi::WifiSubpageEnableToggle(),
                         /*is_checked=*/true),

      Log("Disable WiFi from WiFi subpage"),

      ClickElement(kOSSettingsId, settings::wifi::WifiSubpageEnableToggle()),
      WaitForToggleState(kOSSettingsId,
                         settings::wifi::WifiSubpageEnableToggle(), false),
      WaitForState(kWifiPoweredState, false),
      WaitForElementDisplayNone(kOSSettingsId,
                                settings::wifi::WiFiSubpageNetworkListDiv()),

      Log("Enable WiFi from WiFi subpage"),

      ClickElement(kOSSettingsId, settings::wifi::WifiSubpageEnableToggle()),
      WaitForToggleState(kOSSettingsId,
                         settings::wifi::WifiSubpageEnableToggle(), true),
      WaitForState(kWifiPoweredState, true),
      WaitForElementExists(
          kOSSettingsId, settings::wifi::WiFiSubpageSearchForNetworksSpinner()),
      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::WiFiSubpageSearchForNetworks(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_SCANNING_MESSAGE)),
      WaitForElementDisplayNotNone(kOSSettingsId,
                                   settings::wifi::WiFiSubpageNetworkListDiv()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(WifiInteractiveUiTest,
                       ToggleAndCheckQuickSettingsElements) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kWifiPoweredState);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ToggleObserver, kToggleButtonState);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(NetworkNameObserver, kNetworkInListState);

  ConfigureWifi(/*connected=*/true);

  // Set this delay so the WiFi scanning progress bar shows.
  ShillManagerClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::Seconds(2));

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequence(
      ObserveState(kWifiPoweredState,
                   std::make_unique<ShillDevicePowerStateObserver>(
                       ShillManagerClient::Get(), NetworkTypePattern::WiFi())),
      PollViewProperty(kToggleButtonState,
                       kNetworkDetailedViewWifiToggleElementId,
                       &views::ToggleButton::GetIsOn),
      PollNetworkInList(WifiServiceName(), kNetworkInListState),

      Log("Opening the Quick Settings bubble and navigating to the network "
          "page"),

      OpenQuickSettings(), NavigateQuickSettingsToNetworkPage(),

      Log("Waiting for the network page to be shown and WiFi to have the "
          "expected enabled state"),

      WaitForShow(kNetworkDetailedViewWifiToggleElementId),
      VerifyWifiState(/*enabled=*/true, kWifiPoweredState, kToggleButtonState),

      Log("Verify the WiFi service in the network list"),

      WaitForState(kNetworkInListState, true),

      Log("Disable WiFi"),

      WaitForShow(kNetworkDetailedViewWifiToggleElementId),
      MoveMouseTo(kNetworkDetailedViewWifiToggleElementId), ClickMouse(),

      Log("Wait for WiFi to have the expected disabled state"),

      VerifyWifiState(/*enabled=*/false, kWifiPoweredState, kToggleButtonState),

      Log("Re-enable WiFi"),

      WaitForShow(kNetworkDetailedViewWifiToggleElementId),
      MoveMouseTo(kNetworkDetailedViewWifiToggleElementId), ClickMouse(),

      Log("Wait for WiFi to have the expected enabled state"),

      VerifyWifiState(/*enabled=*/true, kWifiPoweredState, kToggleButtonState),

      Log("Verify the WiFi service in the network list"),

      WaitForState(kNetworkInListState, true),

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

IN_PROC_BROWSER_TEST_F(WifiInteractiveUiTest, ToggleWifiFromInternetPage) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kWifiPoweredState);
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
      WaitForToggleState(kOSSettingsId, settings::wifi::WifiSummaryItemToggle(),
                         /*is_checked=*/true),

      Log("Navigate to the Internet subpage"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Turn off WiFi toggle button from network summary"),

      ClickElement(kOSSettingsId, settings::wifi::WifiSummaryItemToggle()),
      WaitForToggleState(kOSSettingsId, settings::wifi::WifiSummaryItemToggle(),
                         false),
      WaitForState(kWifiPoweredState, false),

      Log("WiFi subpage arrow should disappear"),

      WaitForElementDisplayNone(kOSSettingsId,
                                settings::wifi::WifiSummaryItemSubpageArrow()),

      Log("Add WiFi div in expand section should disappear"),

      ClickElement(kOSSettingsId, settings::AddConnectionsExpandButton()),
      WaitForElementDoesNotExist(kOSSettingsId, settings::AddWiFiRow()),

      Log("Verify WiFi network state is off"),

      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::WifiSummaryItemNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_SETTINGS_DEVICE_OFF)),

      Log("Turn on WiFi toggle button from network summary"),

      ClickElement(kOSSettingsId, settings::wifi::WifiSummaryItemToggle()),
      WaitForToggleState(kOSSettingsId, settings::wifi::WifiSummaryItemToggle(),
                         true),
      WaitForState(kWifiPoweredState, true),

      Log("Verify subpage arrow exists"),

      WaitForElementExists(kOSSettingsId,
                           settings::wifi::WifiSummaryItemSubpageArrow()),

      Log("Verify expand section contains Add Wi-Fi row"),

      WaitForElementExists(kOSSettingsId,
                           settings::AddConnectionsExpandButton()),

      Log("Add WiFi div in expand section should exist"),

      WaitForElementExists(kOSSettingsId, settings::AddWiFiRow()),
      WaitForElementExists(kOSSettingsId, settings::AddWifiIcon()),

      Log("WiFi network state should change from Off to \"No network\" when no "
          "visible network"),

      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::WifiSummaryItemNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_LIST_NO_NETWORK)),

      // Add a Wifi configuration but don't connect it.
      Do([&]() { ConfigureWifi(false); }),

      Log("WiFi network state should change from \"No network\" to \"Not "
          "connected\" when there are available networks"),

      WaitForElementTextContains(
          kOSSettingsId, settings::wifi::WifiSummaryItemNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_LIST_NOT_CONNECTED)),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
