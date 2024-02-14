// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/firmware_update_ui/url_constants.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"

namespace ash {

namespace {

class FirmwareUpdateInteractiveUiTest : public InteractiveAshTest {
 public:
  FirmwareUpdateInteractiveUiTest() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirmwareUpdatesAppWebContentsId);
    webcontents_id_ = kFirmwareUpdatesAppWebContentsId;
  }

  auto LaunchFirmwareUpdatesApp() {
    return Do(
        [&]() { CreateBrowserWindow(GURL(kChromeUIFirmwareUpdateAppURL)); });
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    fwupd_client_ = FwupdClient::Get();
    CHECK(fwupd_client_);
  }

  void TearDownOnMainThread() override {
    fwupd_client_ = nullptr;

    InteractiveAshTest::TearDownOnMainThread();
  }

  auto TriggerFwupdPropertiesChange(uint32_t percentage, FwupdStatus status) {
    return Do([this, percentage, status]() {
      DCHECK(fwupd_client());
      fwupd_client()->TriggerPropertiesChangeForTesting(
          percentage, static_cast<uint32_t>(status));
    });
  }

  auto TriggerSuccessfulUpdate() {
    return Steps(Do([this]() {
      DCHECK(fwupd_client());
      fwupd_client()->TriggerSuccessfulUpdateForTesting();
    }));
  }

  FwupdClient* fwupd_client() const { return fwupd_client_; }

 protected:
  ui::ElementIdentifier webcontents_id_;

 private:
  raw_ptr<FwupdClient> fwupd_client_ = nullptr;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kFwupdClientUpdateState);

IN_PROC_BROWSER_TEST_F(FirmwareUpdateInteractiveUiTest,
                       TestFirmwareUpdateV2FullUpdate) {
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

  const DeepQuery kUpdateDialogProgressQuery{
      "firmware-update-app",
      "firmware-update-dialog",
      "#progress",
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
          Log("Waiting for FwupdClient to register the update..."),
          PollState(kFwupdClientUpdateState,
                    [this]() {
                      return fwupd_client()->HasUpdateStartedForTesting();
                    }),
          WaitForState(kFwupdClientUpdateState, true),
          Log("Triggering Fwupd properties change."),
          TriggerFwupdPropertiesChange(/*percentage=*/50,
                                       /*status=*/FwupdStatus::kDeviceWrite),
          Log("Waiting for update dialog progress to match expected value..."),
          WaitForElementTextContains(webcontents_id_,
                                     kUpdateDialogProgressQuery,
                                     "Updating (50% complete)"),
          Log("Triggering successful update."), TriggerSuccessfulUpdate(),
          Log("Verifying existence of update done button."),
          WaitForElementExists(webcontents_id_,
                               kUpdateDialogDoneButtonQuery))));
}

}  // namespace
}  // namespace ash
