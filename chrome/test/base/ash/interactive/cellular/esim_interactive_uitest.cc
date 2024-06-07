// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/cellular/esim_util.h"
#include "chrome/test/base/ash/interactive/cellular/wait_for_service_connected_observer.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

class EsimInteractiveUITest : public InteractiveAshTest {
 protected:
  EsimInteractiveUITest() : euicc_info_(/*id=*/0), esim_info_(/*id=*/0) {}

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings app is installed.
    InstallSystemApps();

    auto* hermes_manager_client =
        HermesManagerClient::Get()->GetTestInterface();
    ASSERT_TRUE(hermes_manager_client);

    hermes_manager_client->ClearEuiccs();

    hermes_manager_client->AddEuicc(dbus::ObjectPath(euicc_info_.path()),
                                    euicc_info_.eid(),
                                    /*is_active=*/true,
                                    /*physical_slot=*/0);

    auto* hermes_euicc_client = HermesEuiccClient::Get()->GetTestInterface();
    ASSERT_TRUE(hermes_euicc_client);

    hermes_euicc_client->AddCarrierProfile(
        dbus::ObjectPath(esim_info_.profile_path()),
        dbus::ObjectPath(euicc_info_.path()), esim_info_.iccid(),
        esim_info_.name(), esim_info_.nickname(), esim_info_.service_provider(),
        hermes_euicc_client->GenerateFakeActivationCode(),
        /*network_service_path=*/esim_info_.service_path(),
        /*state=*/hermes::profile::State::kPending,
        /*profile_class=*/hermes::profile::ProfileClass::kOperational,
        /*add_carrier_profile_behavior=*/
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithoutService);

    hermes_euicc_client->SetNextRefreshSmdxProfilesResult(
        {dbus::ObjectPath(esim_info_.profile_path())});

    // Make Hermes operations take 5 seconds to complete.
    hermes_euicc_client->SetInteractiveDelay(base::Seconds(5));
  }

  const EsimInfo& esim_info() { return esim_info_; }

 private:
  const EuiccInfo euicc_info_;
  const EsimInfo esim_info_;
};

IN_PROC_BROWSER_TEST_F(EsimInteractiveUITest, InstallProfileWithSMDS) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(WaitForServiceConnectedObserver,
                                      kConnectedToCellularService);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateSettingsToInternetPage(kOSSettingsId),

      Log("Waiting for cellular summary item to exist then click it"),

      WaitForElementExists(kOSSettingsId,
                           settings::cellular::CellularSummaryItem()),
      ClickElement(kOSSettingsId, settings::cellular::CellularSummaryItem()),

      Log("Waiting for \"add eSIM\" button to be enabled then click it"),

      WaitForElementEnabled(kOSSettingsId, settings::cellular::AddEsimButton()),
      ClickElement(kOSSettingsId, settings::cellular::AddEsimButton()),

      Log("Wait for the dialog to open then start the SM-DS scan"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogTitle(),
          /*text=*/
          l10n_util::GetStringUTF8(
              IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_TITLE)),
      WaitForElementEnabled(kOSSettingsId,
                            settings::cellular::EsimDialogForwardButton()),
      ClickElement(kOSSettingsId,
                   settings::cellular::EsimDialogForwardButton()),
      WaitForElementDisabled(kOSSettingsId,
                             settings::cellular::EsimDialogForwardButton()),

      Log("Wait for profiles to be discovered then choose one to install"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogTitle(),
          /*text=*/
          l10n_util::GetStringUTF8(
              IDS_CELLULAR_SETUP_PROFILE_DISCOVERY_PAGE_TITLE)),
      WaitForElementHasAttribute(kOSSettingsId,
                                 settings::cellular::EsimDialogFirstProfile(),
                                 /*attribute=*/"selected"),
      WaitForElementEnabled(kOSSettingsId,
                            settings::cellular::EsimDialogForwardButton()),
      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogForwardButton(),
          /*text=*/
          l10n_util::GetStringUTF8(IDS_CELLULAR_SETUP_NEXT_LABEL)),
      ClickElement(kOSSettingsId,
                   settings::cellular::EsimDialogForwardButton()),

      Log("Wait for the installation to start"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogInstallingMessage(),
          /*text=*/
          l10n_util::GetStringUTF8(
              IDS_CELLULAR_SETUP_ESIM_PROFILE_INSTALLING_MESSAGE)),

      Log("Wait for the Shill service to be created then connect to it"),

      ObserveState(kConnectedToCellularService,
                   std::make_unique<WaitForServiceConnectedObserver>(
                       NetworkHandler::Get()->network_state_handler(),
                       esim_info().iccid())),
      WaitForState(kConnectedToCellularService, true),

      Log("Wait for the installation to finish then close the dialog"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogTitle(),
          /*text=*/
          l10n_util::GetStringUTF8(
              IDS_CELLULAR_SETUP_ESIM_FINAL_PAGE_SUCCESS_HEADER)),
      WaitForElementEnabled(kOSSettingsId,
                            settings::cellular::EsimDialogForwardButton()),
      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogForwardButton(),
          /*text=*/
          l10n_util::GetStringUTF8(IDS_CELLULAR_SETUP_DONE_LABEL)),
      ClickElement(kOSSettingsId,
                   settings::cellular::EsimDialogForwardButton()),

      WaitForElementDoesNotExist(kOSSettingsId,
                                 settings::cellular::EsimDialog()),

      Log("Closing Settings app"),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
