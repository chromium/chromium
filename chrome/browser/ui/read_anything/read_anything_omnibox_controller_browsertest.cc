// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_omnibox_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/action_id.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_constants.h"

using read_anything::ReadAnythingEntryPointController;
using ui_test_utils::NavigateToURL;

class ReadAnythingOmniboxControllerTestBase
    : public InProcessBrowserTest,
      public page_actions::PageActionObserver,
      public testing::WithParamInterface<bool> {
 public:
  ReadAnythingOmniboxControllerTestBase()
      : PageActionObserver(kActionSidePanelShowReadAnything) {}

  void TearDown() override {
    ReadAnythingEntryPointController::ResetCheckCountForTesting();
  }

  bool IsImmersiveEnabled() const { return GetParam(); }

  void VerifyUIState() {
    if (IsImmersiveEnabled()) {
      auto* controller =
          ReadAnythingController::From(browser()->GetActiveTabInterface());
      ASSERT_EQ(controller->GetPresentationState(),
                ReadAnythingController::PresentationState::kInImmersiveOverlay);
    } else {
      ASSERT_TRUE(base::test::RunUntil(
          [&]() { return IsReadAnythingEntryShowing(browser()); }));
    }
  }

  void OpenRMWithOmnibox() {
    actions::ActionInvocationContext context;
    context.SetProperty(page_actions::kPageActionTriggerKey, 1);
    ReadAnythingEntryPointController::InvokePageAction(browser(), context);
  }

  void RegisterPageActionObserver() {
    auto* page_action_controller = browser()
                                       ->GetActiveTabInterface()
                                       ->GetTabFeatures()
                                       ->page_action_controller();
    CHECK(page_action_controller);
    RegisterAsPageActionObserver(*page_action_controller);
  }

  void ExpectPageActionStateImmediate(bool expected_state) {
    EXPECT_EQ(GetCurrentPageActionState().showing, expected_state);
  }

  void WaitForPageActionShowing(bool expected_state) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetCurrentPageActionState().showing == expected_state;
    }));
  }

  void WaitForChipShowing(bool expected_state) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetCurrentPageActionState().chip_showing == expected_state;
    }));
  }

  void ShowPageAction() {
    tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
    tab->GetTabFeatures()->page_action_controller()->Show(
        kActionSidePanelShowReadAnything);
  }

  void MockLongDwellTime() { MockDwellTime(base::Seconds(5)); }

  void MockShortDwellTime() { MockDwellTime(base::Milliseconds(100)); }

  void MockDwellTime(base::TimeDelta time_delta) {
    base::TimeTicks time = base::TimeTicks::Now() - time_delta;
    if (IsImmersiveEnabled()) {
      auto* controller =
          ReadAnythingController::From(browser()->GetActiveTabInterface());
      controller->SetDwellTimeForTesting(time);
    } else {
      auto* controller = side_panel_controller();
      controller->SetDwellTimeForTesting(time);
    }
  }

  void WaitForDebounce() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
    run_loop.Run();
  }

  void NavigateToDistillablePage() {
    if (!embedded_test_server()->Started()) {
      ASSERT_TRUE(embedded_test_server()->Start());
    }
    GURL url = embedded_test_server()->GetURL("/long_text_page.html");
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
        ->AddHintForTesting(
            url, optimization_guide::proto::READER_MODE_ELIGIBLE,
            std::optional<optimization_guide::OptimizationMetadata>());
    EXPECT_TRUE(NavigateToURL(browser(), url));
  }

  int GetOmniboxIgnoredCount() {
    PrefService* prefs = browser()->GetProfile()->GetPrefs();
    return prefs->GetInteger(
        prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount);
  }

  ReadAnythingSidePanelController* side_panel_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->read_anything_side_panel_controller();
  }

  SidePanelEntry* read_anything_entry() {
    return SidePanelRegistry::From(browser()->GetActiveTabInterface())
        ->GetEntryForKey(
            SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  }

  void Activate(std::optional<SidePanelOpenTrigger> trigger) {
    SidePanelEntry* entry = read_anything_entry();
    entry->set_last_open_trigger(trigger);
    OnEntryShown(entry);
  }

  void OnEntryShown(SidePanelEntry* entry) {
    if (IsImmersiveEnabled()) {
      std::optional<ReadAnythingOpenTrigger> read_anything_trigger;
      if (entry->last_open_trigger().has_value()) {
        read_anything_trigger =
            read_anything::SidePanelToReadAnythingOpenTrigger(
                entry->last_open_trigger().value());
      }
      ReadAnythingController::From(browser()->GetActiveTabInterface())
          ->OnEntryShown(read_anything_trigger);
    } else {
      side_panel_controller()->OnEntryShown(entry);
    }
  }

  void Deactivate(ReadAnythingCloseReason reason) {
    if (IsImmersiveEnabled()) {
      auto* read_anything_controller =
          ReadAnythingController::From(browser()->GetActiveTabInterface());
      CHECK(read_anything_controller);
      read_anything_controller->ShowImmersiveUI(
          ReadAnythingOpenTrigger::kReadAnythingContextMenu);
      read_anything_controller->CloseImmersiveUI(reason);
      read_anything_controller->SetPresentationState(
          ReadAnythingController::PresentationState::kInactive);
    } else {
      side_panel_controller()->OnEntryHidden(read_anything_entry());
    }
  }
};

class ReadAnythingOmniboxControllerBrowserTest
    : public InteractiveFeaturePromoTestMixin<
          ReadAnythingOmniboxControllerTestBase> {
 public:
  ReadAnythingOmniboxControllerBrowserTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHReadingModePageActionLabelFeature})) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingOmniboxChip, features::kPageActionsMigration,
        feature_engagement::kIPHReadingModePageActionLabelFeature,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        features::kWasmTtsEngineAutoInstallDisabled
#endif
    };
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsImmersiveEnabled()) {
      enabled_features.push_back(features::kImmersiveReadAnything);
    } else {
      disabled_features.push_back(features::kImmersiveReadAnything);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PrimaryPageChanged_ShowsChipOnDistillablePage) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingOmniboxControllerBrowserTest,
    PrimaryPageChanged_ShowsIconOnDistillablePageAfterIgnoredManyTimes) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForChipShowing(true);
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 5);

  MockLongDwellTime();
  NavigateToDistillablePage();

  WaitForChipShowing(false);
  ExpectPageActionStateImmediate(true);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PrimaryPageChanged_HidesOnNonHttp) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  EXPECT_TRUE(NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  WaitForPageActionShowing(false);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PrimaryPageChanged_HidesOnKnownPoorlyDistilledSites) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.youtube.com")));

  WaitForPageActionShowing(false);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PrimaryPageChanged_UpdatesIgnoredCount) {
  RegisterPageActionObserver();
  // When the page changes with no previous page, ignored count stays at 0.
  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.example.com")));
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);

  // Show the omnibox chip on this page and dwell on it for long enough. The
  // ignored count is still 0.
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
  MockLongDwellTime();
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);

  // After changing pages again, the ignored count should increment because the
  // omnibox entrypoint was showing on the previous page and was dwelled on for
  // a non-trivial amount of time.
  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.youtube.com")));
  EXPECT_EQ(GetOmniboxIgnoredCount(), 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PrimaryPageChanged_DoesNotUpdateIgnoredCountIfRMOpened) {
  RegisterPageActionObserver();
  // When the page changes with no previous page, ignored count stays at 0.
  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.example.com")));
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);

  // Show the omnibox chip on this page and dwell on it for long enough. The
  // ignored count is still 0.
  ShowPageAction();
  MockLongDwellTime();
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);

  // Now when the page changes after RM is opened, the chip is not considered
  // ignored.
  OpenRMWithOmnibox();
  VerifyUIState();
  WaitForPageActionShowing(false);
  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.youtube.com")));
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingOmniboxControllerBrowserTest,
    PrimaryPageChanged_DoesNotUpdateIgnoredCountIfPageNotDwelledOn) {
  // When the page changes with no previous page, ignored count stays at 0.
  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.example.com")));
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);

  // Show the omnibox chip on this page and dwell only briefly.
  ShowPageAction();
  MockShortDwellTime();
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);

  // After changing pages again, the ignored count should still be 0 since the
  // user was not on the previous page long enough to read anything.
  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.youtube.com")));
  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PrimaryPageChangedWithIphShowing_LogsNotOpenedAfterIph) {
  base::HistogramTester histogram_tester;
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.youtube.com")));
  WaitForPageActionShowing(false);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", false, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingOmniboxControllerBrowserTest,
    PrimaryPageChangedWithNoIphShowing_DoesNotLogOpenedAfterIph) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(NavigateToURL(browser(), GURL("https://www.youtube.com")));
  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PrimaryPageChanged_DoesNotCheckIfRMOpened) {
  RegisterPageActionObserver();
  OpenRMWithOmnibox();
  VerifyUIState();

  // It's easier to test this in SP mode so that page changes don't close RM.
  if (IsImmersiveEnabled()) {
    auto* controller =
        ReadAnythingController::From(browser()->GetActiveTabInterface());
    controller->TogglePresentation(/*is_user_initiated=*/true);
  }
  ReadAnythingEntryPointController::ResetCheckCountForTesting();

  NavigateToDistillablePage();
  WaitForDebounce();
  ExpectPageActionStateImmediate(false);
  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       PageChangeWithLoadingIsDebounced) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;

  // Navigate to a new page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  mocked_task_runner->RunUntilIdle();

  // Advance time by 500ms. The 1s timer should not have fired.
  mocked_task_runner->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 0);

  // Navigate again. This should restart the timer.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.support.google.com"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  mocked_task_runner->RunUntilIdle();

  // Advance time by another 700ms. Total 1.2s since first nav, but only 700ms
  // since second nav. Timer should still not have fired if it restarted.
  mocked_task_runner->FastForwardBy(base::Milliseconds(700));
  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 0);

  // Advance remaining 300ms for the second navigation.
  mocked_task_runner->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       TabForegroundedIsDebounced) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  ReadAnythingEntryPointController::ResetCheckCountForTesting();

  // Switch tabs in quick succession.
  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->ActivateTabAt(0);

  // After the last switch, wait until the page action shows. It should have
  // only shown once despite foregrounding the distillable page several times.
  WaitForPageActionShowing(true);
  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       TabForegroundedDoesNotCheckIfAlreadyChecked) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
  EXPECT_GT(ReadAnythingEntryPointController::CheckCountForTesting(), 0);
  ReadAnythingEntryPointController::ResetCheckCountForTesting();

  // Switch tabs in quick succession.
  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->ActivateTabAt(0);

  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       TabForegroundedDoesNotCheckIfRMOpened) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  OpenRMWithOmnibox();
  VerifyUIState();

  // Switch to tab 1.
  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1);
  WaitForDebounce();
  ReadAnythingEntryPointController::ResetCheckCountForTesting();

  // Switch back to tab 0 where RM should still be open. After it loads and
  // debounces, no checks should run.
  browser()->tab_strip_model()->ActivateTabAt(0);
  VerifyUIState();
  WaitForDebounce();
  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       TabBackgrounded_DoesNotCheck) {
  NavigateToDistillablePage();
  ReadAnythingEntryPointController::ResetCheckCountForTesting();

  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1);
  WaitForDebounce();

  EXPECT_EQ(ReadAnythingEntryPointController::CheckCountForTesting(), 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       TabBackgrounded_LogsNotOpenedAfterIPH) {
  base::HistogramTester histogram_tester;
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", false, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       TabDetached_UpdatesIgnoredCountIfPageWasDistillable) {
  chrome::NewTab(browser());
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
  MockLongDwellTime();

  browser()->tab_strip_model()->GetActiveTab()->Close();

  EXPECT_EQ(GetOmniboxIgnoredCount(), 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingOmniboxControllerBrowserTest,
    TabDetached_ShowsIconOnDistillablePageAfterIgnoredManyTimes) {
  chrome::NewTab(browser());
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForChipShowing(true);
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 5);

  // This is the 6th time the chip was ignored.
  MockLongDwellTime();
  browser()->tab_strip_model()->GetActiveTab()->Close();
  WaitForChipShowing(false);

  // Open a new tab and navigate to a distillable page. Only the icon should
  // show.
  chrome::NewTab(browser());
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
  EXPECT_FALSE(GetCurrentPageActionState().chip_showing);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingOmniboxControllerBrowserTest,
    TabDetached_DoesNotUpdateIgnoredCountIfPageWasNotDistillable) {
  chrome::NewTab(browser());
  MockLongDwellTime();

  browser()->tab_strip_model()->GetActiveTab()->Close();

  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingOmniboxControllerBrowserTest,
    TabDetached_DoesNotUpdateIgnoredCountIfPageWasNotChecked) {
  chrome::NewTab(browser());
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
  MockLongDwellTime();

  // Move to the next page without waiting for the load to finish. The ignored
  // count should increment since the previous page was distillable.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return GetOmniboxIgnoredCount() == 1; }));

  // Close the tab before the new page is checked. The ignored count should not
  // increase because it wasn't checked.
  browser()->tab_strip_model()->GetActiveTab()->Close();
  EXPECT_EQ(GetOmniboxIgnoredCount(), 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingOmniboxControllerBrowserTest,
    TabDetached_DoesNotUpdateIgnoredCountIfPageWasNotDwelledOn) {
  chrome::NewTab(browser());
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
  MockShortDwellTime();

  browser()->tab_strip_model()->GetActiveTab()->Close();

  EXPECT_EQ(GetOmniboxIgnoredCount(), 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       TabDetached_LogsNotOpenedAfterIPH) {
  base::HistogramTester histogram_tester;
  chrome::NewTab(browser());
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  browser()->tab_strip_model()->GetActiveTab()->Close();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", false, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       LogsNotOpenedAfterIphTimeout) {
  base::HistogramTester histogram_tester;
  RegisterPageActionObserver();
  NavigateToDistillablePage();

  // The IPH should show once the page action shows.
  WaitForPageActionShowing(true);

  // It shouldn't be logged right away.
  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", 0);
  // After a timeout, the IPH will disappear and this will be logged.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", false) == 1;
  }));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       ActivateWithIphShowing_LogsOpenedAfterIph) {
  base::HistogramTester histogram_tester;
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  OpenRMWithOmnibox();
  VerifyUIState();
  WaitForPageActionShowing(false);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", true, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       ActivateWithNoIphShowing_DoesNotLogOpenedAfterIph) {
  base::HistogramTester histogram_tester;
  OpenRMWithOmnibox();
  VerifyUIState();
  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       Activate_LogsOmniboxEntrypointAfterOmniboxClicked) {
  base::HistogramTester histogram_tester;
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  Activate(SidePanelOpenTrigger::kReadAnythingOmniboxChip);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.EntryPointAfterOmnibox",
      ReadAnythingOpenTrigger::kOmniboxChip, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       Activate_LogsNotOmniboxEntrypointAfterOmniboxShown) {
  base::HistogramTester histogram_tester;
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  Activate(SidePanelOpenTrigger::kReadAnythingContextMenu);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.EntryPointAfterOmnibox",
      ReadAnythingOpenTrigger::kReadAnythingContextMenu, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       Activate_DoesNotLogTogglePresentationAfterOmniboxShown) {
  base::HistogramTester histogram_tester;
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  Activate(SidePanelOpenTrigger::kReadAnythingTogglePresentationButton);

  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.EntryPointAfterOmnibox", 0);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       Activate_HidesOmniboxImmediately) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  Activate(SidePanelOpenTrigger::kReadAnythingTogglePresentationButton);

  ExpectPageActionStateImmediate(false);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       DeactivateByUser_ShowsOmnibox) {
  RegisterPageActionObserver();
  Activate(SidePanelOpenTrigger::kReadAnythingOmniboxChip);
  ExpectPageActionStateImmediate(false);

  Deactivate(ReadAnythingCloseReason::kClosedByUser);

  ExpectPageActionStateImmediate(true);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       DeactivateOnTabChange_DoesNotShowOmnibox) {
  RegisterPageActionObserver();
  Activate(SidePanelOpenTrigger::kReadAnythingOmniboxChip);
  ExpectPageActionStateImmediate(false);

  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1);

  ExpectPageActionStateImmediate(false);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       DeactivateOnPageChange_DoesNotShowOmnibox) {
  // Only relevant with immersive since SP does not close on page change.
  if (IsImmersiveEnabled()) {
    RegisterPageActionObserver();
    Activate(SidePanelOpenTrigger::kReadAnythingOmniboxChip);
    ExpectPageActionStateImmediate(false);

    Deactivate(ReadAnythingCloseReason::kPageChanged);

    ExpectPageActionStateImmediate(false);
  }
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerBrowserTest,
                       OnDiscardContents_ResetsState) {
  RegisterPageActionObserver();
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);

  // Switch to a new tab to background the first tab, so it can be discarded.
  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Discard the first tab.
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  browser()->tab_strip_model()->DiscardWebContentsAt(0,
                                                     std::move(new_contents));

  // Switch back to the discarded tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // The chip should be hidden now because was_page_checked_ was reset.
  // It will eventually show again once the new contents finishes loading and
  // the debounce timer fires.
  ExpectPageActionStateImmediate(false);

  // Verify that it eventually shows again on the new contents.
  // We need to navigate to a distillable page again because the discarded tab
  // starts at about:blank or similar.
  NavigateToDistillablePage();
  WaitForPageActionShowing(true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ReadAnythingOmniboxControllerBrowserTest,
                         testing::Bool());
