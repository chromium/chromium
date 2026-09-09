// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/startup_launch_infobar_delegate.h"
#include "chrome/browser/ui/startup/startup_launch_infobar_manager_impl.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
}

class StartupLaunchInfoBarInteractiveTest : public InteractiveBrowserTest {
 protected:
  StartupLaunchInfoBarInteractiveTest() = default;
  ~StartupLaunchInfoBarInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    manager_ = std::make_unique<StartupLaunchInfoBarManagerImpl>();
  }

  void TearDownOnMainThread() override {
    manager_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<StartupLaunchInfoBarManagerImpl> manager_;
};

IN_PROC_BROWSER_TEST_F(StartupLaunchInfoBarInteractiveTest, ShowOptInInfoBar) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      InstrumentTab(kWebContentsElementId, 0), Do([this]() {
        manager_->ShowInfoBars(
            StartupLaunchInfoBarManager::InfoBarType::kForegroundOptIn);
      }),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      PressButton(ConfirmInfoBar::kOkButtonElementId),
      CheckResult(
          []() {
            return g_browser_process->local_state()->GetBoolean(
                prefs::kForegroundLaunchOnLogin);
          },
          true),
      Do([&histogram_tester]() {
        histogram_tester.ExpectTotalCount(
            "Startup.Launch.InfoBar.ForegroundOptIn.Shown", 1);
        histogram_tester.ExpectUniqueSample(
            "Startup.Launch.InfoBar.ForegroundOptIn.Interaction",
            StartupLaunchInfoBarInteraction::kAccept, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(StartupLaunchInfoBarInteractiveTest, ShowOptOutInfoBar) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      InstrumentTab(kWebContentsElementId, 0), Do([this]() {
        manager_->ShowInfoBars(
            StartupLaunchInfoBarManager::InfoBarType::kForegroundOptOut);
      }),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      PressButton(ConfirmInfoBar::kOkButtonElementId),
      WaitForWebContentsNavigation(kWebContentsElementId,
                                   GURL("chrome://settings/onStartup")),
      Do([&histogram_tester]() {
        histogram_tester.ExpectTotalCount(
            "Startup.Launch.InfoBar.ForegroundOptOut.Shown", 1);
        histogram_tester.ExpectUniqueSample(
            "Startup.Launch.InfoBar.ForegroundOptOut.Interaction",
            StartupLaunchInfoBarInteraction::kAccept, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(StartupLaunchInfoBarInteractiveTest,
                       InfoBarAppearsOnNewTabs) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      Do([this]() {
        manager_->ShowInfoBars(
            StartupLaunchInfoBarManager::InfoBarType::kForegroundOptIn);
      }),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      AddInstrumentedTab(kWebContentsElementId, GURL("about:blank")),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), Do([&histogram_tester]() {
        histogram_tester.ExpectTotalCount(
            "Startup.Launch.InfoBar.ForegroundOptIn.Shown", 1);
      }));
}

IN_PROC_BROWSER_TEST_F(StartupLaunchInfoBarInteractiveTest,
                       DismissingOneClosesAll) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      Do([this]() {
        manager_->ShowInfoBars(
            StartupLaunchInfoBarManager::InfoBarType::kForegroundOptIn);
      }),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      AddInstrumentedTab(kWebContentsElementId, GURL("about:blank")),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      PressButton(ConfirmInfoBar::kDismissButtonElementId),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId),
      // Check that it's also gone from the other tab.
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId),
      Do([&histogram_tester]() {
        histogram_tester.ExpectTotalCount(
            "Startup.Launch.InfoBar.ForegroundOptIn.Shown", 1);
        histogram_tester.ExpectUniqueSample(
            "Startup.Launch.InfoBar.ForegroundOptIn.Interaction",
            StartupLaunchInfoBarInteraction::kDismiss, 1);
      }));
}


IN_PROC_BROWSER_TEST_F(StartupLaunchInfoBarInteractiveTest,
                       InfoBarDoesNotAppearOnIncognitoNewTabs) {
  base::HistogramTester histogram_tester;

  // Create an Incognito browser before showing the infobar.
  BrowserWindowInterface* incognito =
      CreateIncognitoBrowser(browser()->GetProfile());
  EXPECT_TRUE(incognito->GetProfile()->IsIncognitoProfile());

  RunTestSequence(
      Do([this]() {
        manager_->ShowInfoBars(
            StartupLaunchInfoBarManager::InfoBarType::kForegroundOptIn);
      }),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      InContext(BrowserElements::From(incognito)->GetContext(),
                EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId)));
}

class StartupLaunchInfoBarIsolatedModeInteractiveTest
    : public StartupLaunchInfoBarInteractiveTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupLaunchInfoBarInteractiveTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        enterprise_isolated_mode::switches::
            kForceEnterpriseIsolatedModeReplacesIncognito);
  }
};

IN_PROC_BROWSER_TEST_F(StartupLaunchInfoBarIsolatedModeInteractiveTest,
                       InfoBarDoesNotAppearOnIsolatedModeNewTabs) {
  base::HistogramTester histogram_tester;

  // Create an Isolated Mode browser (using NewIncognitoWindow essentially)
  BrowserWindowInterface* isolated =
      CreateIncognitoBrowser(browser()->GetProfile());
  EXPECT_TRUE(isolated->GetProfile()->IsEnterpriseIsolatedModeProfile());

  RunTestSequence(
      Do([this]() {
        manager_->ShowInfoBars(
            StartupLaunchInfoBarManager::InfoBarType::kForegroundOptIn);
      }),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      InContext(BrowserElements::From(isolated)->GetContext(),
                EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId)));
}
