// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/power_monitor/battery_state_sampler.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/test_support/battery_saver_browser_test_mixin.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_interactive_test_mixin.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/animation_test_api.h"

namespace {
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";
}  // namespace

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceWebContentsElementId);
}  // namespace

class PerformanceSidePanelInteractiveTest
    : public MemorySaverInteractiveTestMixin<
          BatterySaverBrowserTestMixin<InteractiveBrowserTest>> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kSidePanelPinning, features::kChromeRefresh2023,
         performance_manager::features::kPerformanceControlsSidePanel},
        {});
    animation_mode_reset_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
    set_open_about_blank_on_browser_launch(true);
    MemorySaverInteractiveTestMixin::SetUp();
  }

  void SetUpOnMainThread() override {
    MemorySaverInteractiveTestMixin::SetUpOnMainThread();
    SetMemorySaverModeEnabled(true);
    SetBatterySaverModeEnabled(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;
};

IN_PROC_BROWSER_TEST_F(PerformanceSidePanelInteractiveTest,
                       SelectPerformanceSidePanel) {
  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Open the side panel via the app menu
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kPerformanceMenuItem),
      WaitForShow(kSidePanelElementId), FlushEvents());
}

IN_PROC_BROWSER_TEST_F(PerformanceSidePanelInteractiveTest,
                       IconChangesOnBatterySaverModeActive) {
  constexpr char kPerformanceButton[] = "performance_button";
  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kPerformanceMenuItem),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      WaitForShow(kPinnedToolbarActionsContainerElementId),
      NameChildViewByType<
          PinnedToolbarActionsContainer::PinnedActionToolbarButton>(
          kPinnedToolbarActionsContainerElementId, kPerformanceButton),
      WaitForShow(kPerformanceButton), FlushEvents(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSkipPixelTestsReason),
      Screenshot(kPerformanceButton, "BatterySaverActiveToolbarButton",
                 "5053929"));
}

IN_PROC_BROWSER_TEST_F(PerformanceSidePanelInteractiveTest,
                       OpenSidePanelFromAppMenu) {
  const DeepQuery kPathToFirstCardElement{"performance-app",
                                          ".card:nth-of-type(1)"};
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCardIsVisible);
  StateChange card_is_visible;
  card_is_visible.event = kCardIsVisible;
  card_is_visible.where = kPathToFirstCardElement;
  card_is_visible.type = StateChange::Type::kExists;

  RunTestSequence(
      Do([this]() {
        SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser())
            ->SetNoDelaysForTesting(true);
      }),
      MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
      SelectMenuItem(AppMenuModel::kPerformanceMenuItem),
      WaitForHide(AppMenuModel::kPerformanceMenuItem),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      WaitForShow(kPerformanceSidePanelWebViewElementId),
      InstrumentNonTabWebView(kPerformanceWebContentsElementId,
                              kPerformanceSidePanelWebViewElementId),
      WaitForStateChange(kPerformanceWebContentsElementId, card_is_visible),
      CheckJsResultAt(kPerformanceWebContentsElementId, kPathToFirstCardElement,
                      "el => el.tagName.toLowerCase()", "browser-health-card"));
}

IN_PROC_BROWSER_TEST_F(PerformanceSidePanelInteractiveTest,
                       OpenSidePanelFromMemorySaverChip) {
  const DeepQuery kPathToFirstCardElement{"performance-app",
                                          ".card:nth-of-type(1)"};
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCardIsVisible);
  StateChange card_is_visible;
  card_is_visible.event = kCardIsVisible;
  card_is_visible.where = kPathToFirstCardElement;
  card_is_visible.type = StateChange::Type::kExists;

  RunTestSequence(
      Do([this]() {
        SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser())
            ->SetNoDelaysForTesting(true);
      }),
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndReloadTab(0, kFirstTabContents),
      PressButton(kMemorySaverChipElementId), WaitForShow(kSidePanelElementId),
      WaitForShow(kPerformanceSidePanelWebViewElementId),
      InstrumentNonTabWebView(kPerformanceWebContentsElementId,
                              kPerformanceSidePanelWebViewElementId),
      WaitForStateChange(kPerformanceWebContentsElementId, card_is_visible),
      CheckJsResultAt(kPerformanceWebContentsElementId, kPathToFirstCardElement,
                      "el => el.id", "memorySaverCard"));
}
