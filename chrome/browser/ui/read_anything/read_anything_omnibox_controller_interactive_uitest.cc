// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <optional>

#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingOmniboxControllerTest
    : public PageActionInteractiveTestMixin<InteractiveFeaturePromoTest>,
      public testing::WithParamInterface<bool> {
 public:
  template <typename... Args>
  explicit ReadAnythingOmniboxControllerTest(Args&&... args)
      : PageActionInteractiveTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHReadingModePageActionLabelFeature})) {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingOmniboxChip, features::kPageActionsMigration,
        feature_engagement::kIPHReadingModePageActionLabelFeature};
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsImmersiveEnabled()) {
      enabled_features.push_back(features::kImmersiveReadAnything);
    } else {
      disabled_features.push_back(features::kImmersiveReadAnything);
    }
    features_.InitWithFeatures(enabled_features, disabled_features);
    distillable_url_ = embedded_test_server()->GetURL("/long_text_page.html");
    non_distillable_url_ = GURL("chrome://blank");
    InteractiveFeaturePromoTest::SetUp();
  }

  bool IsImmersiveEnabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

  using PageActionInteractiveTestMixin::InvokePageAction;
  using PageActionInteractiveTestMixin::WaitForPageActionChipNotVisible;
  using PageActionInteractiveTestMixin::WaitForPageActionChipVisible;
  using PageActionInteractiveTestMixin::WaitForPageActionIconVisible;

  auto WaitForPageActionChipVisible() {
    MultiStep steps;
    steps += WaitForPageActionChipVisible(kActionSidePanelShowReadAnything);
    return steps;
  }

  auto WaitForPageActionIconVisible() {
    MultiStep steps;
    steps += WaitForPageActionIconVisible(kActionSidePanelShowReadAnything);
    return steps;
  }

  auto WaitForPageActionChipNotVisible() {
    MultiStep steps;
    steps += WaitForPageActionChipNotVisible(kActionSidePanelShowReadAnything);
    return steps;
  }

  auto InvokePageAction() {
    MultiStep steps;
    steps += InvokePageAction(kActionSidePanelShowReadAnything);
    return steps;
  }

  auto CloseTab(ui::ElementIdentifier tab) {
    return InAnyContext(WithElement(tab, [this](ui::TrackedElement* el) {
                          content::WebContents* contents =
                              AsInstrumentedWebContents(el)->web_contents();
                          chrome::CloseWebContents(browser(), contents, true);
                        }).SetMustRemainVisible(false));
  }

  // Mimic dwelling on the current page for 4 seconds.
  auto MockDwellTime(ui::ElementIdentifier tab) {
    return InAnyContext(WithElement(tab, [this](ui::TrackedElement* el) {
      content::WebContents* contents =
          InteractiveFeaturePromoTest::AsInstrumentedWebContents(el)
              ->web_contents();
      tabs::TabInterface* tab_interface =
          browser()->tab_strip_model()->GetTabForWebContents(contents);
      CHECK(tab_interface);
      if (IsImmersiveEnabled()) {
        auto* read_anything_controller =
            ReadAnythingController::From(tab_interface);
        CHECK(read_anything_controller);
        read_anything_controller->SetDwellTimeForTesting(
            base::TimeTicks::Now() - base::Seconds(4));
      } else {
        tab_interface->GetTabFeatures()
            ->read_anything_side_panel_controller()
            ->SetDwellTimeForTesting(base::TimeTicks::Now() - base::Seconds(4));
      }
    }));
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  GURL distillable_url_;
  GURL non_distillable_url_;
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       ShowAndHideOmniboxAfterTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  RunTestSequence(
      // First tab is non-distillable, second tab is distillable.
      InstrumentTab(kFirstTab),
      NavigateWebContents(kFirstTab, non_distillable_url_),
      AddInstrumentedTab(kSecondTab, distillable_url_),

      // Select the second tab, wait for chip to show.
      SelectTab(kTabStripElementId, 1), WaitForPageActionChipVisible(),

      // Select the first tab, wait for chip to hide.
      SelectTab(kTabStripElementId, 0), WaitForPageActionChipNotVisible());
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       ShowOmniboxAsIconOnlyAfterIgnoredCountExceedsThreshold) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      // Shown and ignored (1).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(), MockDwellTime(kActiveTab),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (2).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(), MockDwellTime(kActiveTab),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (3).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(), MockDwellTime(kActiveTab),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (4).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(), MockDwellTime(kActiveTab),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (5).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(), MockDwellTime(kActiveTab),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (6). From now on should show as icon only.
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(), MockDwellTime(kActiveTab),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Now should show as icon only.
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionIconVisible(),
      // The icon is used, and not ignored.
      InvokePageAction(), WaitForPageActionChipNotVisible(),
      // Close RM so the omnibox chip can show again.
      Do([&]() {
        auto context = actions::ActionInvocationContext();
        context.SetProperty(
            kSidePanelOpenTriggerKey,
            static_cast<int>(SidePanelOpenTrigger::kPinnedEntryToolbarButton));
        read_anything::ReadAnythingEntryPointController::InvokePageAction(
            browser(), context);
      }),
      WaitForPageActionChipVisible());
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       ShowOmniboxChipImmediatelyAfterReadingModeClosed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(), InvokePageAction(),
      WaitForPageActionChipNotVisible(),
      // Close RM so the omnibox chip can show again.
      Do([&]() {
        auto context = actions::ActionInvocationContext();
        context.SetProperty(
            kSidePanelOpenTriggerKey,
            static_cast<int>(SidePanelOpenTrigger::kPinnedEntryToolbarButton));
        read_anything::ReadAnythingEntryPointController::InvokePageAction(
            browser(), context);
      }),
      WaitForPageActionChipVisible());
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       TabClose_MarkOmniboxIgnoredIfGoodCandidate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab3);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab4);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab5);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab6);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab7);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab8);
  RunTestSequence(
      // Show the chip on the first tab.
      InstrumentTab(kTab1), NavigateWebContents(kTab1, distillable_url_),
      WaitForWebContentsReady(kTab1), WaitForPageActionChipVisible(),
      AddInstrumentedTab(kTab2, non_distillable_url_, 1),
      AddInstrumentedTab(kTab3, distillable_url_, 2),
      AddInstrumentedTab(kTab4, distillable_url_, 3),
      AddInstrumentedTab(kTab5, distillable_url_, 4),
      AddInstrumentedTab(kTab6, distillable_url_, 5),
      AddInstrumentedTab(kTab7, distillable_url_, 6),
      AddInstrumentedTab(kTab8, distillable_url_, 7),
      WaitForWebContentsReady(kTab2),
      // Move to the second tab and close the first tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), MockDwellTime(kTab1), CloseTab(kTab1),
      WaitForWebContentsReady(kTab3),
      // Move to the third tab and close the second tab, this should not count
      // as ignored since the chip should not show on the second tab.
      SelectTab(kTabStripElementId, 1), CloseTab(kTab2),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab4),
      // Move to the fourth tab and close the third tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), MockDwellTime(kTab3), CloseTab(kTab3),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab5),
      // Move to the fifth tab and close the fourth tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), MockDwellTime(kTab4), CloseTab(kTab4),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab6),
      // Move to the sixth tab and close the fifth tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), MockDwellTime(kTab5), CloseTab(kTab5),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab7),
      // Move to the seventh tab and close the sixth tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), MockDwellTime(kTab6), CloseTab(kTab6),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab8),
      // Move to the eighth tab and close the seventh tab, ignoring the chip for
      // the sixth time, so the omnibox should now show as icon only.
      SelectTab(kTabStripElementId, 1), MockDwellTime(kTab7), CloseTab(kTab7),
      WaitForPageActionIconVisible());
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       ShowAndHideOmniboxAfterNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipVisible(),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipNotVisible());
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       HideOmniboxAfterEntryShown) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(InstrumentTab(kActiveTab),
                  NavigateWebContents(kActiveTab, distillable_url_),
                  WaitForWebContentsReady(kActiveTab),
                  WaitForPageActionChipVisible(), InvokePageAction(),
                  WaitForPageActionChipNotVisible());
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       LogRmOpenedAfterOmniboxIphShown) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipVisible(),
      WaitForPromo(feature_engagement::kIPHReadingModePageActionLabelFeature),
      InvokePageAction(), WaitForPageActionChipNotVisible(), Do([this]() {
        histogram_tester().ExpectUniqueSample(
            "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", true, 1);
      }));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       LogRmNotOpenedAfterOmniboxIphShownAndPageChanged) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipVisible(),
      WaitForPromo(feature_engagement::kIPHReadingModePageActionLabelFeature),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(), Do([this]() {
        histogram_tester().ExpectUniqueSample(
            "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", false, 1);
      }));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       ShowAndHideIphAfterTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  RunTestSequence(
      // First tab is non-distillable, second tab is distillable.
      InstrumentTab(kFirstTab),
      NavigateWebContents(kFirstTab, non_distillable_url_),
      AddInstrumentedTab(kSecondTab, distillable_url_),

      // Select the second tab, wait for promo to show.
      SelectTab(kTabStripElementId, 1),
      WaitForPromo(feature_engagement::kIPHReadingModePageActionLabelFeature),

      // Select the first tab, wait for promo to hide.
      SelectTab(kTabStripElementId, 0),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       LogRmNotOpenedAfterOmniboxIphShownAndTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  RunTestSequence(
      InstrumentTab(kFirstTab),
      NavigateWebContents(kFirstTab, non_distillable_url_),
      AddInstrumentedTab(kSecondTab, distillable_url_),
      SelectTab(kTabStripElementId, 1),
      WaitForPromo(feature_engagement::kIPHReadingModePageActionLabelFeature),
      SelectTab(kTabStripElementId, 0),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Do([this]() {
        histogram_tester().ExpectUniqueSample(
            "Accessibility.ReadAnything.OpenedAfterOmniboxIPH", false, 1);
      }));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       ShowAndHideIphAfterNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),
      // Show the Iph after navigating to a distillable domain.
      WaitForPromo(feature_engagement::kIPHReadingModePageActionLabelFeature),
      // Hide the Iph after navigating to a non-distillable domain.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForWebContentsReady(kActiveTab),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_P(ReadAnythingOmniboxControllerTest,
                       HideIPHAfterEntryShown) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipVisible(),
      WaitForPromo(feature_engagement::kIPHReadingModePageActionLabelFeature),
      InvokePageAction(),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ReadAnythingOmniboxControllerTest,
                         testing::Bool());
