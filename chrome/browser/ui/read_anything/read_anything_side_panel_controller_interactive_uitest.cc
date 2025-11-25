// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
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

class MockReadAnythingSidePanelControllerObserver
    : public ReadAnythingSidePanelController::Observer {
 public:
  MOCK_METHOD(void,
              Activate,
              (bool active, std::optional<ReadAnythingOpenTrigger>),
              (override));
  MOCK_METHOD(void, OnSidePanelControllerDestroyed, (), (override));
  MOCK_METHOD(void, OnTabWillDetach, (), (override));
};

class ReadAnythingSidePanelControllerTest : public InProcessBrowserTest {
 public:
  // Wrapper methods around the ReadAnythingSidePanelController. These do
  // nothing more than keep the below tests less verbose (simple pass-throughs).
  ReadAnythingSidePanelController* side_panel_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->read_anything_side_panel_controller();
  }

  void AddObserver(ReadAnythingSidePanelController::Observer* observer) {
    side_panel_controller()->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingSidePanelController::Observer* observer) {
    side_panel_controller()->RemoveObserver(observer);
  }

  std::optional<ReadAnythingOpenTrigger> empty_trigger() {
    return std::optional<ReadAnythingOpenTrigger>();
  }

 protected:
  MockReadAnythingSidePanelControllerObserver side_panel_controller_observer_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
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

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryShown_ActivateObservers) {
  AddObserver(&side_panel_controller_observer_);
  SidePanelEntry* entry = browser()
                              ->GetActiveTabInterface()
                              ->GetTabFeatures()
                              ->side_panel_registry()
                              ->GetEntryForKey(SidePanelEntry::Key(
                                  SidePanelEntry::Id::kReadAnything));
  entry->set_last_open_trigger(SidePanelOpenTrigger::kReadAnythingOmniboxChip);

  EXPECT_CALL(side_panel_controller_observer_,
              Activate(true, std::optional<ReadAnythingOpenTrigger>(
                                 ReadAnythingOpenTrigger::kOmniboxChip)))
      .Times(1);
  side_panel_controller()->OnEntryShown(entry);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryHidden_ActivateObservers) {
  AddObserver(&side_panel_controller_observer_);
  SidePanelEntry* entry = browser()
                              ->GetActiveTabInterface()
                              ->GetTabFeatures()
                              ->side_panel_registry()
                              ->GetEntryForKey(SidePanelEntry::Key(
                                  SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(side_panel_controller_observer_, Activate(false, empty_trigger()))
      .Times(1);
  side_panel_controller()->OnEntryHidden(entry);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       TabWillDetach_NotfiyObservers) {
  AddObserver(&side_panel_controller_observer_);

  EXPECT_CALL(side_panel_controller_observer_, OnTabWillDetach()).Times(1);
  browser()->GetActiveTabInterface()->Close();
}

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

  auto WaitForPageActionChipVisible() {
    MultiStep steps;
    steps += WaitForPageActionChipVisible(kActionSidePanelShowReadAnything);
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
