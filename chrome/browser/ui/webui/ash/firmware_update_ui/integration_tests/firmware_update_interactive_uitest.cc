// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/json/string_escape.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"

namespace ash {

namespace {

constexpr char kFirmwareUpdatesUrl[] = "chrome://accessory-update";

class FirmwareUpdateInteractiveUiTest : public InteractiveAshTest {
 public:
  FirmwareUpdateInteractiveUiTest() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirmwareUpdatesAppWebContentsId);
    webcontents_id_ = kFirmwareUpdatesAppWebContentsId;
  }

  auto LaunchFirmwareUpdatesApp() {
    return Do([&]() { CreateBrowserWindow(GURL(kFirmwareUpdatesUrl)); });
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();
  }

 protected:
  ui::ElementIdentifier webcontents_id_;
};

IN_PROC_BROWSER_TEST_F(FirmwareUpdateInteractiveUiTest,
                       TestUpdateCardPresence) {
  const DeepQuery kUpdateCardQuery{
      "firmware-update-app",
      "peripheral-updates-list",
      "update-card",
  };

  const DeepQuery kUpdateButtonQuery{
      "firmware-update-app",
      "peripheral-updates-list",
      "update-card",
      "#updateButton",
  };

  const DeepQuery kConfirmationDialogNextButtonQuery{
      "firmware-update-app",
      "firmware-confirmation-dialog",
      "#nextButton",
  };

  const DeepQuery kUpdateDialogDoneButtonQuery{
      "firmware-update-app",
      "firmware-update-dialog",
      "#updateDoneButton",
  };

  RunTestSequence(
      InstrumentNextTab(webcontents_id_, AnyBrowser()),
      LaunchFirmwareUpdatesApp(),
      WaitForWebContentsReady(webcontents_id_,
                              GURL("chrome://accessory-update")),
      InAnyContext(Steps(
          Log("Verifying that the update-card element is present."),
          EnsurePresent(webcontents_id_, kUpdateCardQuery),
          Log("Clicking on the update button."),
          ClickElement(webcontents_id_, kUpdateButtonQuery),
          WaitForElementExists(webcontents_id_,
                               kConfirmationDialogNextButtonQuery),
          Log("Clicking on the start update button."),
          ClickElement(webcontents_id_, kConfirmationDialogNextButtonQuery),
          Log("Verifying existence of Update Done button."),
          WaitForElementExists(webcontents_id_,
                               kUpdateDialogDoneButtonQuery))));
}

}  // namespace
}  // namespace ash
