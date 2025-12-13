// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace installer_downloader {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);

// A valid template; IIDGUID and STATS are substituted at runtime.
constexpr char kUrlTemplate[] =
    "https://example.com/installer.exe?iid=IIDGUID&stats=STATS";

class InstallerDownloaderInteractiveUiTest : public InteractiveBrowserTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        kInstallerDownloader,
        {{kInstallerUrlTemplateParam.name, kUrlTemplate}});
    InteractiveBrowserTest::SetUp();
  }

  InteractiveTestApi::MultiStep ShowInfobarOnNewTab() {
    return Steps(AddInstrumentedTab(kSecondTabContents,
                                    GURL(chrome::kChromeUINewTabURL)),
                 WaitForShow(ConfirmInfoBar::kInfoBarElementId));
  }

  // Switches back to the first tab and verifies that the infobar is gone
  // everywhere.
  InteractiveTestApi::MultiStep VerifyNoInfobarInAnyTab() {
    return Steps(WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                 SelectTab(kTabStripElementId, 0),
                 WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  }

  // Assumes that actual window have infobar visible. As a result, new window
  // will also get the infobar.
  InteractiveTestApi::MultiStep ShowInfobarInNewWindow() {
    return Steps(Do([&]() { CreateBrowser(browser()->profile()); }),
                 WaitForShow(ConfirmInfoBar::kInfoBarElementId));
  }

  // Should be invoked from window 1 and that window just removed infobar.
  InteractiveTestApi::MultiStep VerifyNoInfobarInAnyContext() {
    return Steps(WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                 SelectTab(kTabStripElementId, 0),
                 WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  }

  void TriggerInfobar() {
    g_browser_process->local_state()->SetBoolean(
        prefs::kInstallerDownloaderBypassEligibilityCheck, true);
    g_browser_process->GetFeatures()
        ->installer_downloader_controller()
        ->MaybeShowInfoBar();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       AcceptRemovesInfobarFromAllTabs) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarOnNewTab(),
                  PressButton(ConfirmInfoBar::kOkButtonElementId),
                  VerifyNoInfobarInAnyTab());
}

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       DismissRemovesInfobarFromAllTabs) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarOnNewTab(),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyTab());
}

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       InfobarVisibleInFullscreen) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId), Do([&]() {
                    ui_test_utils::ToggleFullscreenModeAndWait(browser());
                  }),
                  EnsurePresent(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       AcceptRemovesInfobarAcrossWindows) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarInNewWindow(),
                  PressButton(ConfirmInfoBar::kOkButtonElementId),
                  VerifyNoInfobarInAnyContext());
}

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       DismissRemovesInfobarAcrossWindows) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarInNewWindow(),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyContext());
}

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       MetricsAcceptPath) {
  base::HistogramTester histograms;

  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kOkButtonElementId),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.InfobarShown",
                                /*sample=*/1, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Windows.InstallerDownloader.RequestAccepted",
                                /*sample=*/1, /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       MetricsDismissPath) {
  base::HistogramTester histograms;

  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.InfobarShown",
                                /*sample=*/1, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Windows.InstallerDownloader.RequestAccepted",
                                /*sample=*/0, /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(InstallerDownloaderInteractiveUiTest,
                       Metrics_InfobarShownOnceAcrossTabsAndWindows) {
  base::HistogramTester histograms;

  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarOnNewTab(), ShowInfobarInNewWindow(),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyContext());

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.InfobarShown",
                                /*sample=*/1, /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace installer_downloader
