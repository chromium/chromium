// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/cellular/esim_util.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "dbus/object_path.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

const char kNewProfileNickname[] = "newName";

class RenameEsimProfileInteractiveUiTest : public InteractiveAshTest {
 protected:
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
        /*state=*/hermes::profile::State::kActive,
        /*profile_class=*/hermes::profile::ProfileClass::kOperational,
        /*add_carrier_profile_behavior=*/
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);

    ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
        esim_info_.service_path(), shill::kStateProperty,
        base::Value(shill::kStateOnline));
  }

  const EsimInfo& esim_info() { return esim_info_; }

 private:
  const EuiccInfo euicc_info_ = EuiccInfo(/*id=*/0);
  const EsimInfo esim_info_ = EsimInfo(/*id=*/0);
};

IN_PROC_BROWSER_TEST_F(RenameEsimProfileInteractiveUiTest, RenamEsimProfile) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Navigating to the internet page"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Opening cellular rename dialog"),

      WaitForElementExists(kOSSettingsId,
                           ash::settings::NetworkMoreDetailsMenuButton()),
      ClickElement(kOSSettingsId,
                   ash::settings::NetworkMoreDetailsMenuButton()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameButton()),
      ClickElement(kOSSettingsId,
                   ash::settings::cellular::CellularSubpageMenuRenameButton()),
      WaitForElementExists(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialog()),

      Log("Updating cellular network name"),

      ClickElement(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialogInputField()),

      // Clear current eSIM name in input field.
      ExecuteJsAt(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialogInputField(),
          "el => el.value = ''"),
      SendTextAsKeyEvents(kOSSettingsId, kNewProfileNickname), FlushEvents(),
      ClickElement(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialogDoneButton()),
      FlushEvents(),
      WaitForElementDoesNotExist(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialog()),
      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::CellularDetailsSubpageTitle(),
          /*text=*/kNewProfileNickname),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
