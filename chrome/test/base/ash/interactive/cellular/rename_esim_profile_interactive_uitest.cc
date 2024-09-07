// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/cellular/esim_name_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

const char kNewProfileNickname[] = "awesomeName";

class RenameEsimProfileInteractiveUiTest : public EsimInteractiveUiTestBase {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    EsimInteractiveUiTestBase::SetUpOnMainThread();

    esim_info_ = std::make_unique<SimInfo>(/*id=*/0);
    ConfigureEsimProfile(euicc_info(), *esim_info_, /*connected=*/true);
  }

  const SimInfo& esim_info() const { return *esim_info_; }

 private:
  std::unique_ptr<SimInfo> esim_info_;
};

// Reanable when flaky test is fixed. See b/350066292.
IN_PROC_BROWSER_TEST_F(RenameEsimProfileInteractiveUiTest, RenameEsimProfile) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(EsimNameObserver, kNameChangedService);

  ui::ElementContext context =
      LaunchSystemWebApp(SystemWebAppType::SETTINGS, kOSSettingsId);

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      ObserveState(kNameChangedService,
                   std::make_unique<EsimNameObserver>(
                       dbus::ObjectPath(esim_info().profile_path()))),

      Log("Navigating to the internet page"),

      NavigateToInternetDetailsPage(kOSSettingsId,
                                    NetworkTypePattern::Cellular(),
                                    esim_info().nickname()),

      Log("Opening cellular rename dialog"),

      WaitForElementExists(kOSSettingsId,
                           ash::settings::NetworkMoreDetailsMenuButton()),

      Log("Checking initial Network name"),

      WaitForState(kNameChangedService, esim_info().nickname()),

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
      ClearInputFieldValue(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialogInputField()),
      ClickElement(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialogInputField()),
      SendTextAsKeyEvents(kOSSettingsId, kNewProfileNickname),
      ClickElement(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialogDoneButton()),

      Log("Checking Network name is set to the expected value"),

      WaitForState(kNameChangedService, kNewProfileNickname),

      Log("Checking Network name in OS Settings UI is set to the expected "
          "value"),
      WaitForElementDoesNotExist(
          kOSSettingsId,
          ash::settings::cellular::CellularSubpageMenuRenameDialog()),
      WaitForElementTextContains(kOSSettingsId,
                                 settings::InternetSettingsSubpageTitle(),
                                 /*text=*/kNewProfileNickname),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
