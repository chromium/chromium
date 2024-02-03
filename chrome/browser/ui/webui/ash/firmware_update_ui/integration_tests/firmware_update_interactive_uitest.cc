// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"

namespace ash {

namespace {

constexpr char kFirmwareUpdatesUrl[] = "chrome://accessory-update";
class FirmwareUpdateInteractiveUiTest : public InteractiveAshTest {
 public:
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
};

IN_PROC_BROWSER_TEST_F(FirmwareUpdateInteractiveUiTest,
                       TestUpdateCardPresence) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirmwareUpdatesAppWebContentsId);

  const DeepQuery kUpdateCardQuery{
      "firmware-update-app",
      "peripheral-updates-list",
      "update-card",
  };

  RunTestSequence(
      InstrumentNextTab(kFirmwareUpdatesAppWebContentsId, AnyBrowser()),
      LaunchFirmwareUpdatesApp(),
      WaitForWebContentsReady(kFirmwareUpdatesAppWebContentsId,
                              GURL("chrome://accessory-update")),
      InAnyContext(Steps(
          Log("Verifying that the update-card element is present."),
          EnsurePresent(kFirmwareUpdatesAppWebContentsId, kUpdateCardQuery))));
}

}  // namespace
}  // namespace ash
