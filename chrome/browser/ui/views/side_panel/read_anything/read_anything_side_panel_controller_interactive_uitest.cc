// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
constexpr char kDocumentWithNamedElement[] = "/select.html";
}  // namespace

class MockReadAnythingSidePanelControllerObserver
    : public ReadAnythingSidePanelController::Observer {
 public:
  MOCK_METHOD(void, Activate, (bool active), (override));
  MOCK_METHOD(void, OnSidePanelControllerDestroyed, (), (override));
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

  EXPECT_CALL(side_panel_controller_observer_, Activate(true)).Times(1);
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

  EXPECT_CALL(side_panel_controller_observer_, Activate(false)).Times(1);
  side_panel_controller()->OnEntryHidden(entry);
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
    a11y::SetDistillableDomainsForTesting({distillable_url_.host()});
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
