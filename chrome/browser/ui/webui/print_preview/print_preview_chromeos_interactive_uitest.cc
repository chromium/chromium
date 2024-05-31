// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChromeBrowserId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrintPreviewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsWebContentsId);

class PrintPreviewChromeOsInteractiveUiTest : public InteractiveAshTest {
 public:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking.
    SetupContextWidget();

    // Ensure the OS Settings system web apps(SWA) is installed.
    InstallSystemApps();
  }

  auto LaunchBrowser() {
    return Steps(InstrumentNextTab(kChromeBrowserId, AnyBrowser()),
                 Do([&]() { CreateBrowserWindow(GURL("chrome://newtab")); }),
                 WaitForShow(kChromeBrowserId));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewChromeOsInteractiveUiTest,
                       OpenPrinterSettingsFromDestinationDialog) {
  const DeepQuery kDestinationSettings{"print-preview-app", "#sidebar",
                                       "#destinationSettings"};

  const DeepQuery kDestinationDropdown = kDestinationSettings +
                                         "#destinationSelect" + "#dropdown" +
                                         "#destinationDropdown";

  const DeepQuery kSeeMoreButton = kDestinationSettings + "#destinationSelect" +
                                   "#dropdown" + "button:nth-child(5)";

  const DeepQuery kManagePrintersButton =
      kDestinationSettings + "print-preview-destination-dialog-cros" +
      "print-preview-printer-setup-info-cros" + "cr-button";

  RunTestSequence(
      Log("Launch Chrome browser"), LaunchBrowser(),
      Log("Send Ctrl + P shortcut to open print preview"),
      SendAccelerator(
          kChromeBrowserId,
          ui::Accelerator(ui::KeyboardCode::VKEY_P, ui::EF_CONTROL_DOWN)),
      WaitForShow(kConstrainedDialogWebViewElementId),
      InstrumentNonTabWebView(kPrintPreviewId,
                              kConstrainedDialogWebViewElementId),
      WaitForElementExists(kPrintPreviewId, kDestinationDropdown),
      Log("Click the See more button to open the destination dialog"),
      ClickElement(kPrintPreviewId, kDestinationDropdown),
      WaitForElementToRender(kPrintPreviewId, kSeeMoreButton),
      ClickElement(kPrintPreviewId, kSeeMoreButton),
      Log("Click the Manage printers button to open the printer settings"),
      WaitForElementExists(kPrintPreviewId, kManagePrintersButton),
      WaitForElementToRender(kPrintPreviewId, kManagePrintersButton),
      InstrumentNextTab(kSettingsWebContentsId, AnyBrowser()),
      ClickElement(kPrintPreviewId, kManagePrintersButton),
      Log("Verify printer settings is open"),
      WaitForWebContentsReady(
          kSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrintingDetailsSubpagePath)));
}

}  // namespace
}  // namespace ash
