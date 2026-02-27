// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "url/url_constants.h"

class ReadAnythingEntryPointControllerTestBase
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsImmersiveEnabled() const { return GetParam(); }

  void VerifyUIState() {
    if (IsImmersiveEnabled()) {
      auto* controller =
          ReadAnythingController::From(browser()->GetActiveTabInterface());
      ASSERT_EQ(controller->GetPresentationState(),
                ReadAnythingController::PresentationState::kInImmersiveOverlay);
    } else {
      auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
      ASSERT_TRUE(base::test::RunUntil([&]() {
        return side_panel_ui->IsSidePanelEntryShowing(
            SidePanelEntryKey(SidePanelEntryId::kReadAnything));
      }));
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ReadAnythingEntryPointControllerBrowserTest
    : public ReadAnythingEntryPointControllerTestBase {
 public:
  ReadAnythingEntryPointControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(features::kImmersiveReadAnything,
                                              IsImmersiveEnabled());
  }
};

class ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest
    : public ReadAnythingEntryPointControllerTestBase {
 public:
  ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    disabled_features.push_back(features::kReadAnythingOmniboxChip);

    if (IsImmersiveEnabled()) {
      enabled_features.push_back(features::kImmersiveReadAnything);
    } else {
      disabled_features.push_back(features::kImmersiveReadAnything);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
};

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest,
    ShowSidePanelFromOmnibox_DoesNothingWithFlagDisabled) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);

  read_anything::ReadAnythingEntryPointController::InvokePageAction(browser(),
                                                                    context);

  histogram_tester.ExpectTotalCount("SidePanel.ReadAnything.ShowTriggered", 0);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest,
    OnPageActionIgnored_DoesNothingWithFlagDisabled) {
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);

  read_anything::ReadAnythingEntryPointController::OnPageActionIgnored(
      browser());

  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->HasPrefPath(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerBrowserTest,
                       ShowSidePanelFromPinned) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, -1);
  context.SetProperty(
      kSidePanelOpenTriggerKey,
      static_cast<int>(SidePanelOpenTrigger::kPinnedEntryToolbarButton));

  read_anything::ReadAnythingEntryPointController::InvokePageAction(browser(),
                                                                    context);

  VerifyUIState();
  if (!IsImmersiveEnabled()) {
    histogram_tester.ExpectUniqueSample(
        "SidePanel.ReadAnything.ShowTriggered",
        SidePanelOpenTrigger::kPinnedEntryToolbarButton, 1);
  }
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.ShowTriggered",
      ReadAnythingOpenTrigger::kPinnedSidePanelEntryToolbarButton, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerBrowserTest,
                       ShowSidePanelFromAppMenu) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  read_anything::ReadAnythingEntryPointController::ShowUI(
      browser(), ReadAnythingOpenTrigger::kAppMenu);

  VerifyUIState();
  if (!IsImmersiveEnabled()) {
    histogram_tester.ExpectUniqueSample("SidePanel.ReadAnything.ShowTriggered",
                                        SidePanelOpenTrigger::kAppMenu, 1);
  }
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.ShowTriggered",
      ReadAnythingOpenTrigger::kAppMenu, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerBrowserTest,
                       ShowSidePanelFromContextMenu) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));

  read_anything::ReadAnythingEntryPointController::ShowUI(
      browser(), ReadAnythingOpenTrigger::kReadAnythingContextMenu);

  VerifyUIState();
  if (!IsImmersiveEnabled()) {
    histogram_tester.ExpectUniqueSample(
        "SidePanel.ReadAnything.ShowTriggered",
        SidePanelOpenTrigger::kReadAnythingContextMenu, 1);
  }
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.ShowTriggered",
      ReadAnythingOpenTrigger::kReadAnythingContextMenu, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ReadAnythingEntryPointControllerBrowserTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    All,
    ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest,
    testing::Bool());

class ReadAnythingEntryPointControllerOmniboxBrowserTest
    : public InProcessBrowserTest {
 public:
  ReadAnythingEntryPointControllerOmniboxBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingOmniboxChip, features::kPageActionsMigration},
        {features::kImmersiveReadAnything});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       ShowSidePanelFromOmnibox) {
  base::HistogramTester histogram_tester;
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);

  read_anything::ReadAnythingEntryPointController::InvokePageAction(browser(),
                                                                    context);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  histogram_tester.ExpectUniqueSample(
      "SidePanel.ReadAnything.ShowTriggered",
      SidePanelOpenTrigger::kReadAnythingOmniboxChip, 1);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.ShowTriggered",
      ReadAnythingOpenTrigger::kOmniboxChip, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       ShowSidePanelFromOmnibox_ResetsIgnoredCount) {
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 3);

  read_anything::ReadAnythingEntryPointController::InvokePageAction(browser(),
                                                                    context);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kReadAnything));
  }));
  EXPECT_EQ(0, browser()->GetProfile()->GetPrefs()->GetInteger(
                   prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       CheckIfShouldSuggestReadingMode_RunsHeuristic) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  static bool called_back = false;
  auto result_callback =
      base::BindOnce([](bool is_good_candidate) { called_back = true; });

  read_anything::ReadAnythingEntryPointController::
      CheckIfShouldSuggestReadingMode(browser(), std::move(result_callback));

  ASSERT_TRUE(base::test::RunUntil([&]() { return called_back; }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       CheckIfShouldSuggestReadingMode_NonHttpIsNotCandidate) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  static bool is_good_candidate_ = true;
  auto result_callback = base::BindOnce(
      [](bool is_good_candidate) { is_good_candidate_ = is_good_candidate; });

  read_anything::ReadAnythingEntryPointController::
      CheckIfShouldSuggestReadingMode(browser(), std::move(result_callback));

  ASSERT_TRUE(base::test::RunUntil([&]() { return !is_good_candidate_; }));
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_DeniedDomainIsNotCandidate) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.docs.google.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  static bool is_good_candidate_ = true;
  auto result_callback = base::BindOnce(
      [](bool is_good_candidate) { is_good_candidate_ = is_good_candidate; });

  read_anything::ReadAnythingEntryPointController::
      CheckIfShouldSuggestReadingMode(browser(), std::move(result_callback));

  ASSERT_TRUE(base::test::RunUntil([&]() { return !is_good_candidate_; }));
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingModeNaive_ReturnsFalseForNonHttp) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_FALSE(read_anything::ReadAnythingEntryPointController::
                   CheckIfShouldSuggestReadingModeNaive(browser()));
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingModeNaive_ReturnsFalseForDenyList) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.docs.google.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_FALSE(read_anything::ReadAnythingEntryPointController::
                   CheckIfShouldSuggestReadingModeNaive(browser()));
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingModeNaive_ReturnsTrueForAllowedDomains) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.blog.google.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_TRUE(read_anything::ReadAnythingEntryPointController::
                  CheckIfShouldSuggestReadingModeNaive(browser()));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       OnPageActionIgnored_IncrementsIgnoredCount) {
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 3);

  read_anything::ReadAnythingEntryPointController::OnPageActionIgnored(
      browser());

  EXPECT_EQ(4, browser()->GetProfile()->GetPrefs()->GetInteger(
                   prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount));
}

// In order to test that Omnibox isn't used in automated tests,
// an embedded_test_server needs to be set up in SetUpOnMainThread.
// Since this isn't needed for the rest of the omnibox tests, this is handled
// in a separate test subclass.
class ReadAnythingEntryPointControllerOmniboxAutomationBrowserTest
    : public ReadAnythingEntryPointControllerOmniboxBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableAutomation);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(
    ReadAnythingEntryPointControllerOmniboxAutomationBrowserTest,
    CheckIfShouldSuggestReadingMode_AutomationEnabledIsNotCandidate) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/dom_distiller/simple_article.html")));

  bool is_good_candidate = true;
  base::RunLoop run_loop;
  auto result_callback = base::BindOnce(
      [](bool* result_out, base::RunLoop* run_loop, bool result_in) {
        *result_out = result_in;
        run_loop->Quit();
      },
      &is_good_candidate, &run_loop);

  read_anything::ReadAnythingEntryPointController::
      CheckIfShouldSuggestReadingMode(browser(), std::move(result_callback));
  run_loop.Run();

  EXPECT_FALSE(is_good_candidate);
}
