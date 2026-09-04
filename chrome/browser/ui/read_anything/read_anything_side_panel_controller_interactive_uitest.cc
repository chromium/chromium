// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"

#include <atomic>
#include <optional>

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"

using read_anything::mojom::ReadAnythingOpenTrigger;

namespace {
constexpr char kDocumentWithNamedElement[] = "/select.html";
}  // namespace

class MockReadAnythingLifecycleObserver : public ReadAnythingLifecycleObserver {
 public:
  MOCK_METHOD(void,
              Activate,
              (bool active,
               ReadAnythingOpenTrigger,
               std::optional<base::TimeDelta>),
              (override));
  MOCK_METHOD(void, OnDestroyed, (), (override));
};

class ReadAnythingSidePanelControllerTest : public InProcessBrowserTest {
 public:
  void AddObserver(ReadAnythingLifecycleObserver* observer) {
    ReadAnythingController::From(browser()->GetActiveTabInterface())
        ->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingLifecycleObserver* observer) {
    ReadAnythingController::From(browser()->GetActiveTabInterface())
        ->RemoveObserver(observer);
  }

  void OnEntryShown(SidePanelEntry* entry) {
    ReadAnythingOpenTrigger read_anything_trigger =
        entry->last_open_trigger().has_value()
            ? read_anything::SidePanelToReadAnythingOpenTrigger(
                  entry->last_open_trigger().value())
            : ReadAnythingOpenTrigger::kUnknown;
    ReadAnythingController::From(browser()->GetActiveTabInterface())
        ->OnEntryShown(read_anything_trigger);
  }

  void OnEntryHidden(SidePanelEntry* entry) {
    ReadAnythingController::From(browser()->GetActiveTabInterface())
        ->OnEntryHidden();
  }

 protected:
  MockReadAnythingLifecycleObserver read_anything_observer_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       RegisterReadAnythingEntry) {
  // The tab should have a read anything entry in its side panel.
  EXPECT_EQ(SidePanelRegistry::From(browser()->GetActiveTabInterface())
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryShown_ActivateObservers) {
  AddObserver(&read_anything_observer_);
  SidePanelEntry* entry =
      SidePanelRegistry::From(browser()->GetActiveTabInterface())
          ->GetEntryForKey(
              SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  entry->set_last_open_trigger(SidePanelOpenTrigger::kReadAnythingOmniboxChip);

  EXPECT_CALL(read_anything_observer_,
              Activate(true, ReadAnythingOpenTrigger::kOmniboxChip, testing::_))
      .Times(1);
  OnEntryShown(entry);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryHidden_ActivateObservers) {
  AddObserver(&read_anything_observer_);
  SidePanelEntry* entry =
      SidePanelRegistry::From(browser()->GetActiveTabInterface())
          ->GetEntryForKey(
              SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(read_anything_observer_,
              Activate(false, ReadAnythingOpenTrigger::kUnknown, testing::_))
      .Times(1);
  OnEntryHidden(entry);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       TabWillDetach_MarkOmniboxIgnoredIfGoodCandidate) {
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

class ReadAnythingSidePanelControllerInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }
  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(
    ReadAnythingSidePanelControllerInteractiveTest,
    OpenImmersiveChangeToSidePanelAndCloseWithKeyboardShortcut) {
  ui::Accelerator reading_mode_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SHOW_READING_MODE_KEYBOARD, &reading_mode_accelerator));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, embedded_test_server()->GetURL(
                                          kDocumentWithNamedElement)),

      // Use the keyboard shortcut command to open the immersive overlay
      SendAccelerator(kBrowserViewElementId, reading_mode_accelerator),

      // Verify that presentation state is Immersive
      CheckResult(
          [this]() {
            return ReadAnythingController::From(
                       browser()->GetTabStripModel()->GetActiveTab())
                ->GetPresentationState();
          },
          ReadAnythingController::PresentationState::kInImmersiveOverlay),

      // Change presentation to Side Panel
      Do([this]() {
        auto* controller = ReadAnythingController::From(
            browser()->GetTabStripModel()->GetActiveTab());
        controller->ShowSidePanelUI(
            SidePanelOpenTrigger::kReadAnythingTogglePresentationButton);
      }),
      WaitForShow(kSidePanelElementId),

      // Verify that presentation state is Side Panel
      CheckResult(
          [this]() {
            return ReadAnythingController::From(
                       browser()->GetTabStripModel()->GetActiveTab())
                ->GetPresentationState();
          },
          ReadAnythingController::PresentationState::kInSidePanel),

      // Use the keyboard shortcut command again to close the side panel
      SendAccelerator(kBrowserViewElementId, reading_mode_accelerator),
      WaitForHide(kSidePanelElementId),

      // Verify that presentation state is Inactive
      CheckResult(
          [this]() {
            return ReadAnythingController::From(
                       browser()->GetTabStripModel()->GetActiveTab())
                ->GetPresentationState();
          },
          ReadAnythingController::PresentationState::kInactive));
}

class ReadAnythingKeyboardShortcutCUJTest
    : public PageActionInteractiveTestMixin<InteractiveFeaturePromoTest> {
 public:
  template <typename... Args>
  explicit ReadAnythingKeyboardShortcutCUJTest(Args&&... args)
      : PageActionInteractiveTestMixin(
            UseDefaultTrackerAllowingPromos({std::forward<Args>(args)...})) {}
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    distillable_url_ = embedded_test_server()->GetURL("/long_text_page.html");
    non_distillable_url_ = GURL("chrome://blank");
    a11y::SetDistillableDomainsForTesting({distillable_url_.GetHost()});

    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingOmniboxChip,
        feature_engagement::kIPHReadingModeKeyboardShortcutFeature};
    feature_list_.InitAndEnableFeatures(enabled_features);
    ReadAnythingController::SetFreezeDistillationOnCreationForTesting(true);

    InteractiveFeaturePromoTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
        ->AddHintForTesting(
            distillable_url_, optimization_guide::proto::READER_MODE_ELIGIBLE,
            std::optional<optimization_guide::OptimizationMetadata>());
  }
  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ReadAnythingController::SetFreezeDistillationOnCreationForTesting(false);
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

  using PageActionInteractiveTestMixin::InvokePageAction;

  auto WaitForPageActionChipVisible() {
    return PageActionInteractiveTestMixin::WaitForPageActionChipVisible(
        kActionSidePanelShowReadAnything);
  }

  auto WaitForPageActionChipNotVisible() {
    return PageActionInteractiveTestMixin::WaitForPageActionChipNotVisible(
        kActionSidePanelShowReadAnything);
  }

  auto InvokePageAction() {
    return PageActionInteractiveTestMixin::InvokePageAction(
        kActionSidePanelShowReadAnything);
  }

  GURL distillable_url_;
  GURL non_distillable_url_;
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingKeyboardShortcutCUJTest, ShowAndHideIph) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),

      // Open Reading Mode.
      WaitForPageActionChipVisible(), InvokePageAction(),

      // Wait for the promo to show.
      WaitForPromo(feature_engagement::kIPHReadingModeKeyboardShortcutFeature),

      // Hide the Iph by navigating away.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class ReadAnythingPresentationModeCUJTest
    : public PageActionInteractiveTestMixin<InteractiveFeaturePromoTest> {
 public:
  template <typename... Args>
  explicit ReadAnythingPresentationModeCUJTest(Args&&... args)
      : PageActionInteractiveTestMixin(
            UseDefaultTrackerAllowingPromos({std::forward<Args>(args)...})) {}
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    distillable_url_ = embedded_test_server()->GetURL("/long_text_page.html");
    non_distillable_url_ = GURL("chrome://blank");
    a11y::SetDistillableDomainsForTesting({distillable_url_.GetHost()});

    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingOmniboxChip,
        feature_engagement::kIPHReadingModePresentationModeFeature};
    feature_list_.InitAndEnableFeatures(enabled_features);
    ReadAnythingController::SetFreezeDistillationOnCreationForTesting(true);

    InteractiveFeaturePromoTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
        ->AddHintForTesting(
            distillable_url_, optimization_guide::proto::READER_MODE_ELIGIBLE,
            std::optional<optimization_guide::OptimizationMetadata>());
  }
  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ReadAnythingController::SetFreezeDistillationOnCreationForTesting(false);
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

  using PageActionInteractiveTestMixin::InvokePageAction;

  auto WaitForPageActionChipVisible() {
    return PageActionInteractiveTestMixin::WaitForPageActionChipVisible(
        kActionSidePanelShowReadAnything);
  }

  auto WaitForPageActionChipNotVisible() {
    return PageActionInteractiveTestMixin::WaitForPageActionChipNotVisible(
        kActionSidePanelShowReadAnything);
  }

  auto InvokePageAction() {
    return PageActionInteractiveTestMixin::InvokePageAction(
        kActionSidePanelShowReadAnything);
  }

  GURL distillable_url_;
  GURL non_distillable_url_;
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingPresentationModeCUJTest, ShowAndHideIph) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),

      // Open Reading Mode.
      WaitForPageActionChipVisible(), InvokePageAction(),

      // Wait for the promo to show.
      WaitForPromo(feature_engagement::kIPHReadingModePresentationModeFeature),

      // Hide the Iph by navigating away.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadAnythingWebContentsId);
}

using ReadAnythingSidePanelInteractiveTest =
    ReadAnythingSidePanelControllerInteractiveTest;

// Regression test for https://crbug.com/557055820.
// Verifies that the "toggle images" menu button toggles the value in prefs.
IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelInteractiveTest, ToggleImages) {
  ui::Accelerator reading_mode_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SHOW_READING_MODE_KEYBOARD, &reading_mode_accelerator));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  auto* const prefs = browser()->GetProfile()->GetPrefs();
  const bool old_setting =
      prefs->GetBoolean(prefs::kAccessibilityReadAnythingImagesEnabled);

  const DeepQuery kSettingsButtonQuery{"read-anything-app",
                                       "read-anything-toolbar", "#more"};
  const DeepQuery kMediaMenuQuery{"read-anything-app", "read-anything-toolbar",
                                  "settings-menu", "#media"};
  const DeepQuery kToggleImagesQuery{"read-anything-app",
                                     "read-anything-toolbar", "media-menu",
                                     "#images-toggle-button"};

  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, embedded_test_server()->GetURL(
                                          kDocumentWithNamedElement)),

      // Show Read Anything side panel.
      Do([this]() {
        auto* controller = ReadAnythingController::From(
            browser()->GetTabStripModel()->GetActiveTab());
        controller->ShowSidePanelUI(
            SidePanelOpenTrigger::kReadAnythingTogglePresentationButton);
      }),
      WaitForShow(kSidePanelElementId),
      // This is an element in the page.
      InAnyContext(WaitForShow(kReadAnythingSettingsButtonElementId)),
      InSameContext(InstrumentWebContentsContaining(
          kReadAnythingWebContentsId, kReadAnythingSettingsButtonElementId)),
      InAnyContext(
          WaitForElementVisible(kReadAnythingWebContentsId,
                                kSettingsButtonQuery),
          ClickElement(kReadAnythingWebContentsId, kSettingsButtonQuery),
          WaitForElementVisible(kReadAnythingWebContentsId, kMediaMenuQuery),
          ClickElement(kReadAnythingWebContentsId, kMediaMenuQuery),
          WaitForElementVisible(kReadAnythingWebContentsId, kToggleImagesQuery),
          ClickElement(kReadAnythingWebContentsId, kToggleImagesQuery),
          PollUntil(
              [prefs, old_setting]() {
                return prefs->GetBoolean(
                           prefs::kAccessibilityReadAnythingImagesEnabled) !=
                       old_setting;
              },
              "Wait for setting to change.")));
}
