// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

const char kNewProfileNickname[] = "newName";

using RenameEsimProfileInteractiveUiTest = EsimInteractiveUiTestBase;

// Reanable when flaky test is fixed. See b/350066292.
IN_PROC_BROWSER_TEST_F(RenameEsimProfileInteractiveUiTest,
                       DISABLED_RenamEsimProfile) {
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
      WaitForElementTextContains(kOSSettingsId,
                                 settings::SettingsSubpageTitle(),
                                 /*text=*/kNewProfileNickname),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
