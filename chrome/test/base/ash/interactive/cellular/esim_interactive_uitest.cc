// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "ash/ash_element_identifiers.h"
#include "base/check.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/cellular/wait_for_service_connected_observer.h"
#include "chrome/test/base/ash/interactive/network/shill_device_power_state_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

namespace ash {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);
const base::TimeDelta kInhibitPropertyChangeTimeout = base::Seconds(2);

class EsimInteractiveUiTest : public EsimInteractiveUiTestBase {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    esim_info_ = std::make_unique<SimInfo>(/*id=*/0);
  }

  ui::test::internal::InteractiveTestPrivate::MultiStep
  SetDevicePropertyChangeDelay() {
    return Steps(Do([&]() {
      ShillDeviceClient::TestInterface* device_test =
          ShillDeviceClient::Get()->GetTestInterface();
      device_test->SetPropertyChangeDelay(kInhibitPropertyChangeTimeout);
    }));
  }

  const SimInfo& esim_info() const { return *esim_info_; }

 private:
  std::unique_ptr<SimInfo> esim_info_;
};

IN_PROC_BROWSER_TEST_F(EsimInteractiveUiTest,
                       OpenAddEsimDialogFromQuickSettings) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kMobileDataPoweredState);

  ConfigureEsimProfile(euicc_info(), esim_info(), /*connected=*/true);

  using Observer = views::test::PollingViewObserver<bool, views::View>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kPollingViewState);

  bool has_clicked_add_esim_entry = false;

  RunTestSequence(
      Log("Waiting for cellular to be enabled"),

      ObserveState(
          kMobileDataPoweredState,
          std::make_unique<ShillDevicePowerStateObserver>(
              ShillManagerClient::Get(), NetworkTypePattern::Mobile())),
      WaitForState(kMobileDataPoweredState, true),

      Log("Opening Quick Settings and navigating to the network page"),

      OpenQuickSettings(), NavigateQuickSettingsToNetworkPage(),

      Log("Waiting for the 'add eSIM' button to be visible, then clicking it"),

      InstrumentNextTab(kOSSettingsId, AnyBrowser()),

      // The views in the network page of Quick Settings (that are not
      // top-level e.g. the toggles or headers) are prone to frequent
      // re-ordering and/or can rapidly appear/disappear before being stable due
      // to network state changes. Instead of attempting to click the view via
      // moving and clicking the mouse we instead click via code to avoid the
      // possibility of the element disappearing during the step.
      PollView(
          kPollingViewState, kNetworkAddEsimElementId,
          [&has_clicked_add_esim_entry](const views::View* view) -> bool {
            if (!has_clicked_add_esim_entry) {
              views::test::ButtonTestApi(
                  views::Button::AsButton(const_cast<views::View*>(view)))
                  .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed,
                                              gfx::PointF(), gfx::PointF(),
                                              base::TimeTicks(), 0, 0));
            }
            has_clicked_add_esim_entry = true;
            return true;
          },
          base::Milliseconds(50)),

      // The `WaitForState` step also requires that the element in question
      // exists for the duration of the step. As mentioned above, the element
      // may rapidly appear/disappear which would cause `WaitForState` to fail.
      // Instead, we wait for the Quick Settings to close as a result of the
      // button being clicked.

      WaitForHide(ash::kQuickSettingsViewElementId),

      Log("Waiting for OS Settings to open"),

      InAnyContext(WaitForShow(kOSSettingsId)));

  ui::ElementContext context = FindSystemWebApp(SystemWebAppType::SETTINGS);

  // Run the remaining steps with a longer timeout since it can take more than
  // 10 seconds for OS Settings to open.
  const base::test::ScopedRunLoopTimeout longer_timeout(FROM_HERE,
                                                        base::Seconds(15));

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Waiting for OS Settings to navigate to cellular subpage"),

      WaitForElementTextContains(
          kOSSettingsId, settings::InternetSettingsSubpageTitle(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_TYPE_MOBILE_DATA)),

      Log("Waiting for 'add eSIM' dialog to open"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogTitle(),
          /*text=*/
          l10n_util::GetStringUTF8(
              IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_TITLE)),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(EsimInteractiveUiTest, AutoconnectBehavior) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kMobileDataPoweredState);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                      kCellularServiceConnected);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kCellularServiceAutoconnect);

  ConfigureEsimProfile(euicc_info(), esim_info(), /*connected=*/true);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Waiting for cellular to be enabled"),

      ObserveState(
          kMobileDataPoweredState,
          std::make_unique<ShillDevicePowerStateObserver>(
              ShillManagerClient::Get(), NetworkTypePattern::Mobile())),
      WaitForState(kMobileDataPoweredState, true),

      Log("Waiting for cellular network to be connected"),

      ObserveState(kCellularServiceConnected,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info().iccid())),
      WaitForState(kCellularServiceConnected, true),

      Log("Waiting for cellular network to be have auto-connect enabled"),

      PollState(kCellularServiceAutoconnect,
                [this]() -> bool {
                  const auto* shill_service_client_test =
                      ShillServiceClient::Get()->GetTestInterface();
                  CHECK(shill_service_client_test);
                  const auto* cellular_properties =
                      shill_service_client_test->GetServiceProperties(
                          esim_info().service_path());
                  CHECK(cellular_properties);
                  const std::optional<bool> autoconnect =
                      cellular_properties->FindBool(
                          shill::kAutoConnectProperty);
                  CHECK(autoconnect.has_value());
                  return *autoconnect;
                }),
      WaitForState(kCellularServiceAutoconnect, true),

      Log("Navigating to the details page for the cellular network"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),
      WaitForElementTextContains(kOSSettingsId,
                                 settings::InternetSettingsSubpageTitle(),
                                 /*text=*/esim_info().nickname()),

      Log("Disabling auto-connect for the cellular network"),

      WaitForElementEnabled(
          kOSSettingsId,
          settings::cellular::CellularDetailsSubpageAutoConnectToggle()),
      WaitForElementChecked(
          kOSSettingsId,
          settings::cellular::CellularDetailsSubpageAutoConnectToggle()),
      ClickElement(
          kOSSettingsId,
          settings::cellular::CellularDetailsSubpageAutoConnectToggle()),
      WaitForElementUnchecked(
          kOSSettingsId,
          settings::cellular::CellularDetailsSubpageAutoConnectToggle()),
      WaitForState(kCellularServiceAutoconnect, false),
      WaitForElementWithManagedPropertyBoolean(
          kOSSettingsId, settings::InternetDetailsSubpage(),
          /*property=*/"typeProperties.cellular.autoConnect",
          /*expected_value=*/false),

      Log("Enabling auto-connect for the cellular network"),

      ClickElement(
          kOSSettingsId,
          settings::cellular::CellularDetailsSubpageAutoConnectToggle()),
      WaitForElementChecked(
          kOSSettingsId,
          settings::cellular::CellularDetailsSubpageAutoConnectToggle()),
      WaitForState(kCellularServiceAutoconnect, true),

      Log("Closing the Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(EsimInteractiveUiTest,
                       ConnectDisconnectFromDetailsPage) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                      kCellularServiceConnected);

  ConfigureEsimProfile(euicc_info(), esim_info(), /*connected=*/true);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      ObserveState(kCellularServiceConnected,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info().iccid())),
      WaitForState(kCellularServiceConnected, true),

      Log("Navigating to the details page for the eSIM network"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Disconnect eSIM network"),

      WaitForElementExists(kOSSettingsId,
                           settings::SettingsSubpageConnectDisconnectButton()),
      ClickElement(kOSSettingsId,
                   settings::SettingsSubpageConnectDisconnectButton()),
      WaitForState(kCellularServiceConnected, false),

      Log("Connect to eSIM network"),

      WaitForElementExists(kOSSettingsId,
                           settings::SettingsSubpageConnectDisconnectButton()),
      ClickElement(kOSSettingsId,
                   settings::SettingsSubpageConnectDisconnectButton()),
      WaitForState(kCellularServiceConnected, true),

      WaitForElementTextContains(
          kOSSettingsId, settings::SettingsSubpageNetworkState(),
          /*text=*/l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),

      Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(EsimInteractiveUiTest, ConnectFromMobileDataSubpage) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                      kCellularServiceConnected);

  ConfigureEsimProfile(euicc_info(), esim_info(), /*connected=*/false);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      ObserveState(kCellularServiceConnected,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       esim_info().iccid())),
      WaitForState(kCellularServiceConnected, false),

      Log("Navigating to Mobile data subpage"),

      NavigateSettingsToNetworkSubpage(kOSSettingsId,
                                       ash::NetworkTypePattern::Mobile()),

      Log("Connect to eSIM network"),

      // Add a property change delay to allow enough time for test to
      // observe cellular inhibition changes in the UI.
      SetDevicePropertyChangeDelay(),

      ClickAnyElementTextContains(kOSSettingsId,
                                  settings::cellular::CellularNetworksList(),
                                  WebContentsInteractionTestUtil::DeepQuery({
                                      "network-list",
                                      "network-list-item",
                                      "div#divText",
                                  }),
                                  esim_info().nickname()),

      Log("Check cellular is inhibited"),

      WaitForElementDoesNotHaveAttribute(
          kOSSettingsId, settings::cellular::CellularInhibitedItem(), "hidden"),

      Log("Check cellular is no longer inhibited"),

      WaitForElementHasAttribute(
          kOSSettingsId, settings::cellular::CellularInhibitedItem(), "hidden"),

      Log("Check network is connected"),

      WaitForState(kCellularServiceConnected, true),
      WaitForAnyElementTextContains(
          kOSSettingsId, settings::cellular::CellularNetworksList(),
          WebContentsInteractionTestUtil::DeepQuery(
              {"network-list", "network-list-item", "div#sublabel"}),
          l10n_util::GetStringUTF8(IDS_ONC_CONNECTED).c_str()),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
