// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/ash/components/dbus/fwupd/fake_fwupd_client.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"

namespace ash {

namespace {

class FirmwareUpdateInteractiveUiTest : public InteractiveAshTest {
 public:
  FirmwareUpdateInteractiveUiTest() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirmwareUpdatesAppWebContentsId);
    webcontents_id_ = kFirmwareUpdatesAppWebContentsId;

    feature_list_.InitAndEnableFeature(features::kFirmwareUpdateUIV2);
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

    // Ensure that the Fake FwupdClient is running.
    CHECK(FwupdClient::GetFake());
    FwupdClient::GetFake()->set_defer_install_update_callback(true);
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

  auto TriggerDeviceRequestToUnplugReplug() {
    return Steps(Do([this]() {
      DCHECK(fwupd_client());
      // A device_request_id of 1 corresponds to FWUPD_REQUEST_ID_REMOVE_REPLUG
      // in firmware_update.mojom.
      fwupd_client()->EmitDeviceRequestForTesting(/*device_request_id=*/1);
    }));
  }

  FakeFwupdClient* fwupd_client() const { return FwupdClient::GetFake(); }

 protected:
  ui::ElementIdentifier webcontents_id_;
  base::test::ScopedFeatureList feature_list_;
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

  const DeepQuery kUpdateDialogBodyQuery{
      "firmware-update-app",
      "firmware-update-dialog",
      "#updateDialogBody",
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
                    [this]() { return fwupd_client()->has_update_started(); }),
          WaitForState(kFwupdClientUpdateState, true),
          Log("Triggering Fwupd properties change."),
          TriggerFwupdPropertiesChange(/*percentage=*/50,
                                       /*status=*/FwupdStatus::kDeviceWrite),
          Log("Waiting for update dialog progress to match expected value..."),
          WaitForElementTextContains(webcontents_id_,
                                     kUpdateDialogProgressQuery,
                                     "Updating (50% complete)"),
          Log("Triggering device request."),
          TriggerDeviceRequestToUnplugReplug(),
          Log("Triggering Fwupd properties change to WaitingForUser."),
          TriggerFwupdPropertiesChange(/*percentage=*/60,
                                       /*status=*/FwupdStatus::kWaitingForUser),
          Log("Waiting for update dialog progress to match expected value..."),
          WaitForElementTextContains(webcontents_id_,
                                     kUpdateDialogProgressQuery,
                                     "Paused (60% complete)"),
          Log("Waiting for update dialog body to match expected value..."),
          WaitForElementTextContains(webcontents_id_, kUpdateDialogBodyQuery,
                                     "Unplug and replug the fake_device to "
                                     "continue the update process"),
          Log("Triggering successful update."), TriggerSuccessfulUpdate(),
          Log("Verifying existence of update done button."),
          WaitForElementExists(webcontents_id_,
                               kUpdateDialogDoneButtonQuery))));
}

}  // namespace
}  // namespace ash
