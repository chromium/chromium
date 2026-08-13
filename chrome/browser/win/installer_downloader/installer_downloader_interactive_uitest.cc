// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/infobars/infobar_features.h"
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

class InstallerDownloaderInteractiveUiTestBase : public InteractiveBrowserTest {
 protected:
  InteractiveTestApi::MultiStep ShowInfobarOnNewTab() {
    return Steps(AddInstrumentedTab(kSecondTabContents,
                                    chrome::ChromeUINewTabURLAsGURL()),
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
    return Steps(Do([&]() { CreateBrowser(browser()->GetProfile()); }),
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
};

class InstallerDownloaderInteractiveUiTest
    : public InstallerDownloaderInteractiveUiTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    if (GetParam()) {
      enabled_features.push_back({infobars::kCentralizedInfoBarFramework,
                                  {{"MigratedInstallerDownloader", "true"}}});
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
                       AcceptRemovesInfobarFromAllTabs) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarOnNewTab(),
                  PressButton(ConfirmInfoBar::kOkButtonElementId),
                  VerifyNoInfobarInAnyTab());
}

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
                       DismissRemovesInfobarFromAllTabs) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarOnNewTab(),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyTab());
}

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
                       InfobarVisibleInFullscreen) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId), Do([&]() {
                    ui_test_utils::ToggleFullscreenModeAndWait(browser());
                  }),
                  EnsurePresent(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
                       AcceptRemovesInfobarAcrossWindows) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarInNewWindow(),
                  PressButton(ConfirmInfoBar::kOkButtonElementId),
                  VerifyNoInfobarInAnyContext());
}

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
                       DismissRemovesInfobarAcrossWindows) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarInNewWindow(),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyContext());
}

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_P(InstallerDownloaderInteractiveUiTest,
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

INSTANTIATE_TEST_SUITE_P(All,
                         InstallerDownloaderInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Migrated" : "Legacy";
                         });

class InstallerDownloaderReengagementInteractiveUiTest
    : public InstallerDownloaderInteractiveUiTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    if (GetParam()) {
      enabled_features.push_back({infobars::kCentralizedInfoBarFramework,
                                  {{"MigratedInstallerDownloader", "true"}}});
    }
    enabled_features.push_back(
        {kInstallerDownloaderReengagement,
         {{"MaxCycleCount", "3"}, {"ReengagementCooldownDays", "60"}}});
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(InstallerDownloaderReengagementInteractiveUiTest,
                       ReengagementShowsAgainAfterCooldown) {
  base::HistogramTester histograms;

  // 1. Show and dismiss in Cycle 1.
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyTab());

  // Verify dismissed metric.
  histograms.ExpectUniqueSample("Windows.InstallerDownloader.RequestAccepted",
                                /*false=*/0, /*expected_bucket_count=*/1);

  // 2. Try to show again immediately (should fail because in cooldown).
  TriggerInfobar();
  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));

  // 3. Simulate cooldown passed by modifying pref.
  g_browser_process->local_state()->SetTime(
      prefs::kInstallerDownloaderInfobarLastShowTime,
      base::Time::Now() - base::Days(61));

  // 4. Trigger again, should show (Cycle 2).
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyTab());

  // Verify metrics.
  histograms.ExpectUniqueSample("Windows.InstallerDownloader.InfobarShown",
                                /*true=*/1, /*expected_bucket_count=*/2);

  histograms.ExpectBucketCount(
      "Windows.InstallerDownloader.Reengagement.InfobarShown",
      /*cycle 1=*/1, /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "Windows.InstallerDownloader.Reengagement.InfobarShown",
      /*cycle 2=*/2, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(InstallerDownloaderReengagementInteractiveUiTest,
                       ReengagementStopsAfterMaxCycles) {
  base::HistogramTester histograms;

  // Cycle 1: Show and Dismiss.
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyTab());

  // Cycle 2: Fast forward, Show and Dismiss.
  g_browser_process->local_state()->SetTime(
      prefs::kInstallerDownloaderInfobarLastShowTime,
      base::Time::Now() - base::Days(61));
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyTab());

  // Cycle 3 (Last): Fast forward, Show and Dismiss.
  g_browser_process->local_state()->SetTime(
      prefs::kInstallerDownloaderInfobarLastShowTime,
      base::Time::Now() - base::Days(61));
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyTab());

  // Campaign should be ended now. TotalShowCount should be logged.
  // We had 3 shows (1 in C1, 1 in C2, 1 in C3).
  histograms.ExpectUniqueSample("Windows.InstallerDownloader.TotalShowCount",
                                /*sample=*/3, /*expected_bucket_count=*/1);

  // Try to show again after another cooldown (should fail).
  g_browser_process->local_state()->SetTime(
      prefs::kInstallerDownloaderInfobarLastShowTime,
      base::Time::Now() - base::Days(61));
  TriggerInfobar();
  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

INSTANTIATE_TEST_SUITE_P(All,
                         InstallerDownloaderReengagementInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigratedInfobar"
                                             : "LegacyInfobar";
                         });

}  // namespace
}  // namespace installer_downloader
