// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"

namespace {
constexpr char kDocumentWithNamedElement[] = "/select.html";
}  // namespace

class MockReadAnythingLifecycleObserver
    : public ReadAnythingLifecycleObserver {
 public:
  MOCK_METHOD(void,
              Activate,
              (bool active, std::optional<ReadAnythingOpenTrigger>),
              (override));
  MOCK_METHOD(void, OnDestroyed, (), (override));
  MOCK_METHOD(void, OnTabWillDetach, (), (override));
};

class ReadAnythingSidePanelControllerTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ReadAnythingSidePanelControllerTest() {
    feature_list_.InitWithFeatureState(features::kImmersiveReadAnything,
                                       IsImmersiveEnabled());
  }

  // Wrapper methods around the ReadAnythingSidePanelController. These do
  // nothing more than keep the below tests less verbose (simple pass-throughs).
  ReadAnythingSidePanelController* side_panel_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->read_anything_side_panel_controller();
  }

  void AddObserver(ReadAnythingLifecycleObserver* observer) {
    if (IsImmersiveEnabled()) {
      ReadAnythingController::From(browser()->GetActiveTabInterface())
          ->AddObserver(observer);
    } else {
      side_panel_controller()->AddObserver(observer);
    }
  }
  void RemoveObserver(ReadAnythingLifecycleObserver* observer) {
    if (IsImmersiveEnabled()) {
      ReadAnythingController::From(browser()->GetActiveTabInterface())
          ->RemoveObserver(observer);
    } else {
      side_panel_controller()->RemoveObserver(observer);
    }
  }

  std::optional<ReadAnythingOpenTrigger> empty_trigger() {
    return std::optional<ReadAnythingOpenTrigger>();
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

  void OnEntryHidden(SidePanelEntry* entry) {
    if (IsImmersiveEnabled()) {
      ReadAnythingController::From(browser()->GetActiveTabInterface())
          ->OnEntryHidden();
    } else {
      side_panel_controller()->OnEntryHidden(entry);
    }
  }

 protected:
  bool IsImmersiveEnabled() const { return GetParam(); }
  MockReadAnythingLifecycleObserver read_anything_observer_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ReadAnythingSidePanelControllerTest,
                       RegisterReadAnythingEntry) {
  // The tab should have a read anything entry in its side panel.
  EXPECT_EQ(browser()
                ->GetActiveTabInterface()
                ->GetTabFeatures()
                ->side_panel_registry()
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingSidePanelControllerTest,
                       OnEntryShown_ActivateObservers) {
  AddObserver(&read_anything_observer_);
  SidePanelEntry* entry = browser()
                              ->GetActiveTabInterface()
                              ->GetTabFeatures()
                              ->side_panel_registry()
                              ->GetEntryForKey(SidePanelEntry::Key(
                                  SidePanelEntry::Id::kReadAnything));
  entry->set_last_open_trigger(SidePanelOpenTrigger::kReadAnythingOmniboxChip);

  EXPECT_CALL(read_anything_observer_,
              Activate(true, std::optional<ReadAnythingOpenTrigger>(
                                 ReadAnythingOpenTrigger::kOmniboxChip)))
      .Times(1);
  OnEntryShown(entry);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingSidePanelControllerTest,
                       OnEntryHidden_ActivateObservers) {
  AddObserver(&read_anything_observer_);
  SidePanelEntry* entry = browser()
                              ->GetActiveTabInterface()
                              ->GetTabFeatures()
                              ->side_panel_registry()
                              ->GetEntryForKey(SidePanelEntry::Key(
                                  SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(read_anything_observer_, Activate(false, empty_trigger()))
      .Times(1);
  OnEntryHidden(entry);
}

IN_PROC_BROWSER_TEST_P(ReadAnythingSidePanelControllerTest,
                       TabWillDetach_NotfiyObservers) {
  AddObserver(&read_anything_observer_);

  EXPECT_CALL(read_anything_observer_, OnTabWillDetach()).Times(1);
  browser()->GetActiveTabInterface()->Close();
}

IN_PROC_BROWSER_TEST_P(ReadAnythingSidePanelControllerTest,
                       TabWillDetach_MarkOmniboxIgnoredIfGoodCandidate) {
  browser()->GetActiveTabInterface()->Close();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ReadAnythingSidePanelControllerTest,
                         testing::Bool());

class ReadAnythingCUJTest : public InteractiveFeaturePromoTest {
 public:
  template <typename... Args>
  explicit ReadAnythingCUJTest(Args&&... args)
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({std::forward<Args>(args)...})) {}
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    distillable_url_ =
        embedded_test_server()->GetURL(kDocumentWithNamedElement);
    non_distillable_url_ = GURL("chrome://blank");
    a11y::SetDistillableDomainsForTesting({distillable_url_.GetHost()});
    feature_list_.InitWithExistingFeatures(
        {feature_engagement::kIPHReadingModeSidePanelFeature});

    InteractiveFeaturePromoTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }
  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }
  GURL distillable_url_;
  GURL non_distillable_url_;
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingCUJTest, ShowAndHideIphAfterTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  RunTestSequence(
      // First tab is non-distillable, second tab is distillable.
      InstrumentTab(kFirstTab),
      NavigateWebContents(kFirstTab, non_distillable_url_),
      AddInstrumentedTab(kSecondTab, distillable_url_),

      // Select the second tab, wait for promo to show.
      SelectTab(kTabStripElementId, 1),
      WaitForPromo(feature_engagement::kIPHReadingModeSidePanelFeature),

      // Select the first tab, wait for promo to hide.
      SelectTab(kTabStripElementId, 0),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingCUJTest, ShowAndHideIphAfterNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),

      // Show the Iph after navigating to a distillable domain.
      WaitForPromo(feature_engagement::kIPHReadingModeSidePanelFeature),

      // Hide the Iph after navigating to a non-distillable domain.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class ReadAnythingOmniboxTest
    : public PageActionInteractiveTestMixin<InteractiveFeaturePromoTest> {
 public:
  template <typename... Args>
  explicit ReadAnythingOmniboxTest(Args&&... args)
      : PageActionInteractiveTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHReadingModePageActionLabelFeature})) {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    features_.InitWithFeatures(
        {features::kReadAnythingOmniboxChip, features::kPageActionsMigration,
         feature_engagement::kIPHReadingModePageActionLabelFeature},
        {});
    distillable_url_ = embedded_test_server()->GetURL("/long_text_page.html");
    non_distillable_url_ = GURL("chrome://blank");
    InteractiveFeaturePromoTest::SetUp();
  }

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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  GURL distillable_url_;
  GURL non_distillable_url_;
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
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

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
                       ShowOmniboxAsIconOnlyAfterIgnoredCountExceedsThreshold) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      // Shown and ignored (1).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (2).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (3).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (4).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (5).
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForPageActionChipNotVisible(),
      // Shown and ignored (6). From now on should show as icon only.
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForPageActionChipVisible(),
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
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForWebContentsReady(kActiveTab),
      // Now should show chip again.
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
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
      SelectTab(kTabStripElementId, 1), CloseTab(kTab1),
      WaitForWebContentsReady(kTab3),
      // Move to the third tab and close the second tab, this should not count
      // as ignored since the chip should not show on the second tab.
      SelectTab(kTabStripElementId, 1), CloseTab(kTab2),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab4),
      // Move to the fourth tab and close the third tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), CloseTab(kTab3),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab5),
      // Move to the fifth tab and close the fourth tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), CloseTab(kTab4),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab6),
      // Move to the sixth tab and close the fifth tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), CloseTab(kTab5),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab7),
      // Move to the seventh tab and close the sixth tab, ignoring the chip.
      SelectTab(kTabStripElementId, 1), CloseTab(kTab6),
      WaitForPageActionChipVisible(), WaitForWebContentsReady(kTab8),
      // Move to the eighth tab and close the seventh tab, ignoring the chip for
      // the sixth time, so the omnibox should now show as icon only.
      SelectTab(kTabStripElementId, 1), CloseTab(kTab7),
      WaitForPageActionChipNotVisible(), WaitForPageActionIconVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
                       ShowAndHideOmniboxAfterNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipVisible(),
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForWebContentsReady(kActiveTab), WaitForPageActionChipNotVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest, HideOmniboxAfterEntryShown) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(InstrumentTab(kActiveTab),
                  NavigateWebContents(kActiveTab, distillable_url_),
                  WaitForWebContentsReady(kActiveTab),
                  WaitForPageActionChipVisible(), InvokePageAction(),
                  WaitForPageActionChipNotVisible());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
                       EntryPointLoggedAfterOmniboxShownAndClicked) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(InstrumentTab(kActiveTab),
                  NavigateWebContents(kActiveTab, distillable_url_),
                  WaitForWebContentsReady(kActiveTab),
                  WaitForPageActionChipVisible(), InvokePageAction(),
                  WaitForPageActionChipNotVisible(), Do([this]() {
                    histogram_tester().ExpectUniqueSample(
                        "Accessibility.ReadAnything.EntryPointAfterOmnibox",
                        ReadAnythingOpenTrigger::kOmniboxChip, 1);
                  }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
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

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
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

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest, ShowAndHideIphAfterTabSwitch) {
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

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest,
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

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest, ShowAndHideIphAfterNavigation) {
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

IN_PROC_BROWSER_TEST_F(ReadAnythingOmniboxTest, HideIPHAfterEntryShown) {
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
