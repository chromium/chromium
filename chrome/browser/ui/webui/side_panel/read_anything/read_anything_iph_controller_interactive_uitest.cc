// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_iph_controller.h"

#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace {

class ReadAnythingIphCujTestBase
    : public PageActionInteractiveTestMixin<InteractiveFeaturePromoTest> {
 public:
  ReadAnythingIphCujTestBase()
      : PageActionInteractiveTestMixin(UseDefaultTrackerAllowingPromos({})) {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    distillable_url_ = embedded_test_server()->GetURL("/long_text_page.html");
    non_distillable_url_ = GURL("chrome://blank");

    feature_list_.InitAndEnableFeatures(GetEnabledFeatures());
    if (ShouldFreezeDistillationOnCreation()) {
      ReadAnythingController::SetFreezeDistillationOnCreationForTesting(true);
    }

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
    if (ShouldFreezeDistillationOnCreation()) {
      ReadAnythingController::SetFreezeDistillationOnCreationForTesting(false);
    }
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

 protected:
  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() const = 0;

  virtual bool ShouldFreezeDistillationOnCreation() const { return false; }

  using PageActionInteractiveTestMixin::InvokePageAction;

  auto WaitForPageActionChipVisible() {
    return PageActionInteractiveTestMixin::WaitForPageActionChipVisible(
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

class ReadAnythingKeyboardShortcutCUJTest : public ReadAnythingIphCujTestBase {
 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    return {features::kReadAnythingOmniboxChip,
            feature_engagement::kIPHReadingModeKeyboardShortcutFeature};
  }

  bool ShouldFreezeDistillationOnCreation() const override { return true; }
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

      // Hide the IPH by navigating away.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class ReadAnythingPresentationModeCUJTest : public ReadAnythingIphCujTestBase {
 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    return {features::kReadAnythingOmniboxChip,
            feature_engagement::kIPHReadingModePresentationModeFeature};
  }

  bool ShouldFreezeDistillationOnCreation() const override { return true; }
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

      // Hide the IPH by navigating away.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class ReadAnythingLineFocusCUJTest : public ReadAnythingIphCujTestBase {
 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    return {features::kReadAnythingLineFocus,
            feature_engagement::kIPHReadingModeLineFocusFeature};
  }
};

IN_PROC_BROWSER_TEST_F(ReadAnythingLineFocusCUJTest, ShowAndHideIph) {
  ui::Accelerator reading_mode_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SHOW_READING_MODE_KEYBOARD, &reading_mode_accelerator));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),

      // Use the keyboard shortcut command to open Reading Mode.
      SendAccelerator(kBrowserViewElementId, reading_mode_accelerator),

      // Wait for the promo to show.
      WaitForPromo(feature_engagement::kIPHReadingModeLineFocusFeature),

      // Hide the IPH by navigating to an empty page.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class ReadAnythingAiPlaybackCUJTest : public ReadAnythingIphCujTestBase {
 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    return {features::kReadAnythingOmniboxChip,
            features::kReadAnythingReadAloudExperimentalPlaybackUi,
            feature_engagement::kIPHReadingModeAiPlaybackFeature,
            feature_engagement::kIPHReadingModeAiPlaybackSettingsFeature};
  }

 private:
  base::AutoReset<base::TimeDelta> ai_playback_iph_delay_ =
      ReadAnythingIphController::SetAiPlaybackIphDelayForTesting(
          base::Seconds(0));
};

IN_PROC_BROWSER_TEST_F(ReadAnythingAiPlaybackCUJTest, ShowAndHideIph) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),

      // Open Reading Mode.
      WaitForPageActionChipVisible(), InvokePageAction(),

      // Wait for the promo to show.
      WaitForPromo(feature_engagement::kIPHReadingModeAiPlaybackFeature),

      // Hide the IPH by navigating away.
      NavigateWebContents(kActiveTab, non_distillable_url_),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAiPlaybackCUJTest,
                       ToggleAiPlaybackShowsSettingsPromo) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, distillable_url_),

      // Open Reading Mode.
      WaitForPageActionChipVisible(), InvokePageAction(),

      // Wait for the AI playback promo to show.
      WaitForPromo(feature_engagement::kIPHReadingModeAiPlaybackFeature),

      // Click the AI playback toggle button.
      InAnyContext(PressButton(kReadAnythingAiPlaybackToggleElementId)),

      // Verify the settings toast promo shows.
      WaitForPromo(
          feature_engagement::kIPHReadingModeAiPlaybackSettingsFeature));
}

}  // namespace
