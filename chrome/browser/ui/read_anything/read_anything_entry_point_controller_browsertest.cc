// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"

#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/accessibility_features.h"
#include "url/url_constants.h"

using read_anything::ReadAnythingEntryPointController;

class TabRemovedWaiter : public TabStripModelObserver {
 public:
  explicit TabRemovedWaiter(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {
    tab_strip_model_->AddObserver(this);
  }
  ~TabRemovedWaiter() override { tab_strip_model_->RemoveObserver(this); }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kRemoved) {
      run_loop_.Quit();
    }
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  raw_ptr<TabStripModel> tab_strip_model_;
};

class ReadAnythingEntryPointControllerTestBase
    : public InProcessBrowserTest,
      public page_actions::PageActionObserver,
      public testing::WithParamInterface<bool> {
 public:
  ReadAnythingEntryPointControllerTestBase()
      : PageActionObserver(kActionSidePanelShowReadAnything) {}

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

  void RegisterPageActionObserver() {
    auto* page_action_controller = browser()
                                       ->GetActiveTabInterface()
                                       ->GetTabFeatures()
                                       ->page_action_controller();
    CHECK(page_action_controller);
    RegisterAsPageActionObserver(*page_action_controller);
  }

  void VerifyPageActionIsShowing(bool expected_state) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetCurrentPageActionState().showing == expected_state;
    }));
  }

  void VerifyChipIsShowing(bool expected_state) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetCurrentPageActionState().chip_showing == expected_state;
    }));
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

  ReadAnythingEntryPointController::InvokePageAction(browser(), context);

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

  ReadAnythingEntryPointController::ShowUI(browser(),
                                           ReadAnythingOpenTrigger::kAppMenu);

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

  ReadAnythingEntryPointController::ShowUI(
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

class ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest
    : public ReadAnythingEntryPointControllerTestBase {
 public:
  ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features = {
        features::kReadAnythingOmniboxChip,
        feature_engagement::kIPHReadingModePageActionLabelFeature};

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
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);

  ReadAnythingEntryPointController::InvokePageAction(browser(), context);

  histogram_tester.ExpectTotalCount("SidePanel.ReadAnything.ShowTriggered", 0);
  histogram_tester.ExpectTotalCount("Accessibility.ReadAnything.ShowTriggered",
                                    0);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest,
    OnPageActionIgnored_DoesNothingWithFlagDisabled) {
  ReadAnythingEntryPointController::OnPageActionIgnored(browser());

  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->HasPrefPath(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount));
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest,
    UpdatePageActionVisibility_DoesNothingWithFlagDisabled) {
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
  RegisterPageActionObserver();

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      true, browser()->GetActiveTabInterface());

  ASSERT_FALSE(GetCurrentPageActionState().showing);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReadAnythingEntryPointControllerOmniboxDisabledBrowserTest,
    testing::Bool());

class ReadAnythingEntryPointControllerOmniboxBrowserTest
    : public InteractiveFeaturePromoTestMixin<
          ReadAnythingEntryPointControllerTestBase> {
 public:
  ReadAnythingEntryPointControllerOmniboxBrowserTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHReadingModePageActionLabelFeature})),
        test_min_pdf_text_length_for_omnibox_(
            ReadAnythingEntryPointController::SetMinPdfTextLengthForTesting(
                500)) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingOmniboxChip,
        feature_engagement::kIPHReadingModePageActionLabelFeature};
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsImmersiveEnabled()) {
      enabled_features.push_back(features::kImmersiveReadAnything);
    } else {
      disabled_features.push_back(features::kImmersiveReadAnything);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::AutoReset<size_t> test_min_pdf_text_length_for_omnibox_;
};

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       ShowSidePanelFromOmnibox) {
  base::HistogramTester histogram_tester;
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);

  ReadAnythingEntryPointController::InvokePageAction(browser(), context);

  VerifyUIState();
  if (!IsImmersiveEnabled()) {
    histogram_tester.ExpectUniqueSample(
        "SidePanel.ReadAnything.ShowTriggered",
        SidePanelOpenTrigger::kReadAnythingOmniboxChip, 1);
  }
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.ShowTriggered",
      ReadAnythingOpenTrigger::kOmniboxChip, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       ShowSidePanelFromOmnibox_ResetsIgnoredCount) {
  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_FALSE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything)));
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 3);

  ReadAnythingEntryPointController::InvokePageAction(browser(), context);

  VerifyUIState();
  EXPECT_EQ(0, browser()->GetProfile()->GetPrefs()->GetInteger(
                   prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       ShowSidePanelFromOmnibox_HidesPromoAsUsed) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<user_education::FeaturePromoResult> future;
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);
  auto* const user_ed = BrowserUserEducationInterface::From(browser());
  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      true, browser()->GetActiveTabInterface(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(user_ed->IsFeaturePromoActive(
      feature_engagement::kIPHReadingModePageActionLabelFeature));

  ReadAnythingEntryPointController::InvokePageAction(browser(), context);

  VerifyUIState();
  histogram_tester.ExpectUniqueSample(
      "UserEducation.MessageAction.IPH_ReadingModePageActionLabel",
      user_education::FeaturePromoClosedReason::kFeatureEngaged, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       UpdatePageActionVisibility_ShowsAndHidesPageAction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/long_text_page.html");
  RegisterPageActionObserver();
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
      ->AddHintForTesting(
          url, optimization_guide::proto::READER_MODE_ELIGIBLE,
          std::optional<optimization_guide::OptimizationMetadata>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  VerifyPageActionIsShowing(true);
  VerifyChipIsShowing(true);

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      false, browser()->GetActiveTabInterface());
  VerifyPageActionIsShowing(false);
  VerifyChipIsShowing(false);

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      true, browser()->GetActiveTabInterface());
  VerifyPageActionIsShowing(true);
  VerifyChipIsShowing(true);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    UpdatePageActionVisibility_DoesNotShowChipIfIgnoredManyTimes) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 10);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/long_text_page.html");
  RegisterPageActionObserver();
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
      ->AddHintForTesting(
          url, optimization_guide::proto::READER_MODE_ELIGIBLE,
          std::optional<optimization_guide::OptimizationMetadata>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  VerifyPageActionIsShowing(true);
  VerifyChipIsShowing(false);

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      false, browser()->GetActiveTabInterface());
  VerifyPageActionIsShowing(false);
  VerifyChipIsShowing(false);

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      true, browser()->GetActiveTabInterface());
  VerifyPageActionIsShowing(true);
  VerifyChipIsShowing(false);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       UpdatePageActionVisibility_ShowsPromo) {
  base::test::TestFuture<user_education::FeaturePromoResult> future;

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      true, browser()->GetActiveTabInterface(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), user_education::FeaturePromoResult::Success());
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       UpdatePageActionVisibility_AbortsPromo) {
  base::HistogramTester histogram_tester;
  auto* const user_ed = BrowserUserEducationInterface::From(browser());
  base::test::TestFuture<user_education::FeaturePromoResult> future;
  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      true, browser()->GetActiveTabInterface(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(user_ed->IsFeaturePromoActive(
      feature_engagement::kIPHReadingModePageActionLabelFeature));

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      false, browser()->GetActiveTabInterface());

  EXPECT_FALSE(user_ed->IsFeaturePromoActive(
      feature_engagement::kIPHReadingModePageActionLabelFeature));
  histogram_tester.ExpectUniqueSample(
      "UserEducation.MessageAction.IPH_ReadingModePageActionLabel",
      user_education::FeaturePromoClosedReason::kAbortedByFeature, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    UpdatePageActionVisibility_DoesNotAbortPromoIfAlreadyHidden) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<user_education::FeaturePromoResult> future;
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);
  auto* const user_ed = BrowserUserEducationInterface::From(browser());
  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      true, browser()->GetActiveTabInterface(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(user_ed->IsFeaturePromoActive(
      feature_engagement::kIPHReadingModePageActionLabelFeature));

  ReadAnythingEntryPointController::InvokePageAction(browser(), context);

  VerifyUIState();
  histogram_tester.ExpectUniqueSample(
      "UserEducation.MessageAction.IPH_ReadingModePageActionLabel",
      user_education::FeaturePromoClosedReason::kFeatureEngaged, 1);
  EXPECT_FALSE(user_ed->IsFeaturePromoActive(
      feature_engagement::kIPHReadingModePageActionLabelFeature));

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      false, browser()->GetActiveTabInterface());
  histogram_tester.ExpectBucketCount(
      "UserEducation.MessageAction.IPH_ReadingModePageActionLabel",
      user_education::FeaturePromoClosedReason::kAbortedByFeature, 0);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    UpdatePageActionVisibility_DoesNotAbortPromoIfNeverShown) {
  base::HistogramTester histogram_tester;
  actions::ActionInvocationContext context;
  context.SetProperty(page_actions::kPageActionTriggerKey, 1);

  ReadAnythingEntryPointController::UpdatePageActionVisibility(
      false, browser()->GetActiveTabInterface());

  histogram_tester.ExpectBucketCount(
      "UserEducation.MessageAction.IPH_ReadingModePageActionLabel",
      user_education::FeaturePromoClosedReason::kAbortedByFeature, 0);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_OptimizationGuideYesAndReadabilityYesIsCandidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/long_text_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
      ->AddHintForTesting(
          url, optimization_guide::proto::READER_MODE_ELIGIBLE,
          std::optional<optimization_guide::OptimizationMetadata>());
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kShowArticle, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_OptimizationGuideNoAndReadabilityYesIsNotCandidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/long_text_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kHideOptimizationGuide, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_OptimizationGuideYesAndReadabilityNoIsNotCandidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
      ->AddHintForTesting(
          url, optimization_guide::proto::READER_MODE_ELIGIBLE,
          std::optional<optimization_guide::OptimizationMetadata>());
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kHideReadability, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_OptimizationGuideNoAndReadabilityNoIsNotCandidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kHideOptimizationGuide, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       CheckIfShouldSuggestReadingMode_LongerPdfIsCandidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/pdf/accessibility/paragraphs-and-heading-untagged.pdf")));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kShowPdf, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_ShorterPdfIsNotCandidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kHideShortPdf, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_LongerPdfWithLotsOfSymbolsIsNotCandidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/pdf/accessibility/paragraphs-and-heading-untagged-nonsense.pdf")));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kHideLowAlphabeticPdf, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       CheckIfShouldSuggestReadingMode_NonHttpIsNotCandidate) {
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kHideNonHttp, 1);
}

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerOmniboxBrowserTest,
    CheckIfShouldSuggestReadingMode_DeniedDomainIsNotCandidate) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.docs.google.com")));
  base::test::TestFuture<bool> future;

  base::HistogramTester histogram_tester;
  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.OmniboxChipDecision",
      ReadAnythingOmniboxChipDecision::kHideDenyList, 1);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       OnPageActionIgnored_IncrementsIgnoredCount) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 3);

  ReadAnythingEntryPointController::OnPageActionIgnored(browser());

  EXPECT_EQ(4, browser()->GetProfile()->GetPrefs()->GetInteger(
                   prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingEntryPointControllerOmniboxBrowserTest,
                       OnPageActionIgnored_HidesChipAfterIgnoredThreshold) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/long_text_page.html");
  RegisterPageActionObserver();
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
      ->AddHintForTesting(
          url, optimization_guide::proto::READER_MODE_ELIGIBLE,
          std::optional<optimization_guide::OptimizationMetadata>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  VerifyChipIsShowing(true);
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 5);

  ReadAnythingEntryPointController::OnPageActionIgnored(browser());

  VerifyChipIsShowing(false);
}

// This test needs to be in a separate test suite because it requires a mock
// OptimizationGuideKeyedService to delay the callback until after WebContents
// destruction, which must be registered before the profile is created.
class ReadAnythingEntryPointControllerTabCloseBrowserTest
    : public ReadAnythingEntryPointControllerTestBase {
 public:
  ReadAnythingEntryPointControllerTabCloseBrowserTest() {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                RegisterMockOptimizationGuideKeyedServiceFactory));

    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingOmniboxChip};
    if (IsImmersiveEnabled()) {
      enabled_features.push_back(features::kImmersiveReadAnything);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUpOnMainThread() override {
    ReadAnythingEntryPointControllerTestBase::SetUpOnMainThread();
    mock_optimization_guide_keyed_service_ =
        static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(
                browser()->profile()));
    ASSERT_TRUE(mock_optimization_guide_keyed_service_);
  }

  void TearDownOnMainThread() override {
    mock_optimization_guide_keyed_service_ = nullptr;
    ReadAnythingEntryPointControllerTestBase::TearDownOnMainThread();
  }

  MockOptimizationGuideKeyedService& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }

 private:
  static void RegisterMockOptimizationGuideKeyedServiceFactory(
      content::BrowserContext* context) {
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
  }

  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_P(
    ReadAnythingEntryPointControllerTabCloseBrowserTest,
    CheckIfShouldSuggestReadingMode_TabClosedBeforeCallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Add a tab so we don't close the browser.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  GURL url = embedded_test_server()->GetURL("/long_text_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  optimization_guide::OptimizationGuideDecisionCallback callback;

  EXPECT_CALL(
      mock_optimization_guide_keyed_service(),
      CanApplyOptimization(
          url, optimization_guide::proto::READER_MODE_ELIGIBLE,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(
          [&](const GURL& url,
              optimization_guide::proto::OptimizationType optimization_type,
              optimization_guide::OptimizationGuideDecisionCallback cb) {
            callback = std::move(cb);
          });

  base::test::TestFuture<bool> future;

  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), future.GetCallback());

  ASSERT_TRUE(callback);

  TabRemovedWaiter waiter(browser()->tab_strip_model());

  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(),
      TabCloseTypes::CLOSE_USER_GESTURE);

  waiter.Wait();

  std::move(callback).Run(optimization_guide::OptimizationGuideDecision::kTrue,
                          optimization_guide::OptimizationMetadata());

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReadAnythingEntryPointControllerTabCloseBrowserTest,
    testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         ReadAnythingEntryPointControllerOmniboxBrowserTest,
                         testing::Bool());

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

IN_PROC_BROWSER_TEST_P(
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

  ReadAnythingEntryPointController::CheckIfShouldSuggestReadingMode(
      browser(), std::move(result_callback));
  run_loop.Run();

  EXPECT_FALSE(is_good_candidate);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReadAnythingEntryPointControllerOmniboxAutomationBrowserTest,
    testing::Bool());
