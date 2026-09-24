// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/callback_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/animations/common_animation_values.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view_shadow.h"
#include "ui/views/view_tracker.h"

namespace {
constexpr int kBrowserWindowWidth = 1400;
constexpr int kBrowserWindowHeight = 800;

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kShowAnimationComplete);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHideAnimationComplete);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCollapseComplete);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kExpandOnHoverComplete);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

std::vector<base::CallbackListSubscription> SubscribeToAnimations(
    BrowserWindowInterface* browser) {
  std::vector<base::CallbackListSubscription> subscriptions;

  subscriptions.emplace_back(
      BrowserAnimationController::From(browser)->Subscribe(
          OrganizerPanelAnimations::kOrganizerPanel,
          base::BindLambdaForTesting([browser](const BrowserAnimationController*
                                                   controller,
                                               BrowserAnimationUpdate update) {
            if (update == BrowserAnimationUpdate::kEnded) {
              const auto motion = controller->GetCurrentMotion(
                  OrganizerPanelAnimations::kOrganizerPanel);
              auto* const browser_view =
                  BrowserView::GetBrowserViewForBrowser(browser);
              if (motion == OrganizerPanelAnimations::kShow) {
                views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
                    kShowAnimationComplete, browser_view);
              } else if (motion == OrganizerPanelAnimations::kHide) {
                views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
                    kHideAnimationComplete, browser_view);
              }
            }
          })));

  subscriptions.emplace_back(
      BrowserAnimationController::From(browser)->Subscribe(
          TabStripAnimations::kVerticalTabStrip,
          base::BindLambdaForTesting([browser](const BrowserAnimationController*
                                                   controller,
                                               BrowserAnimationUpdate update) {
            if (update == BrowserAnimationUpdate::kEnded) {
              const auto motion = controller->GetCurrentMotion(
                  TabStripAnimations::kVerticalTabStrip);
              auto* const browser_view =
                  BrowserView::GetBrowserViewForBrowser(browser);
              if (motion == TabStripAnimations::kExpandOnHover) {
                views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
                    kExpandOnHoverComplete, browser_view);
              }
              if (motion != TabStripAnimations::kExpand) {
                views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
                    kCollapseComplete, browser_view);
              }
            }
          })));

  return subscriptions;
}

}  // namespace

class OrganizerPanelUiTest : public InteractiveBrowserTest {
 public:
  OrganizerPanelUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {organizer_panel::kOrganizerPanel, tabs::kVerticalTabsExpandOnHover},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    browser()->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kTabSearchPinnedToTabstrip, true);

    // Resize the window to be wide enough to accommodate a wide vertical tab
    // strip and the toolbar.
    browser()->GetWindow()->SetBounds(
        gfx::Rect(0, 0, kBrowserWindowWidth, kBrowserWindowHeight));

    animation_subscriptions_ = SubscribeToAnimations(browser());
  }

  void TearDownOnMainThread() override {
    animation_subscriptions_.clear();
    expand_on_hover_lock_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  auto SetVerticalTabsEnabled(bool vertical_tabs_enabled,
                              bool expand_on_hover_enabled = true) {
    auto steps =
        Steps(Do([this, vertical_tabs_enabled, expand_on_hover_enabled]() {
                auto* const controller =
                    tabs::VerticalTabStripStateController::From(browser());
                controller->SetVerticalTabsEnabled(vertical_tabs_enabled);
                controller->SetExpandOnHoverEnabled(expand_on_hover_enabled);
                RunScheduledLayouts();
              }),
              WaitForShow(vertical_tabs_enabled
                              ? kVerticalTabStripTopContainerElementId
                              : kTabSearchButtonElementId));
    AddDescriptionPrefix(steps, "SetVerticalTabsEnabled()");
    return steps;
  }

  auto CollapseTabStrip() {
    auto steps = Steps(PressButton(kVerticalTabStripCollapseButtonElementId),
                       WaitForEvent(kBrowserViewElementId, kCollapseComplete));
    AddDescriptionPrefix(steps, "CollapseTabStrip()");
    return steps;
  }

  auto WaitForExpandOnHover() {
    auto steps =
        Steps(WaitForEvent(kBrowserViewElementId, kExpandOnHoverComplete),
              CheckView(
                  kTabStripRegionElementId,
                  [this](VerticalTabStripRegionView* region) {
                    const bool is_eoh = region->is_expanded_on_hover();
                    expand_on_hover_lock_ = region->GetExpandOnHoverLock(
                        ExpandOnHoverLockType::kKeepExpanded);
                    return is_eoh;
                  },
                  true)
                  .SetDescription("Check and lock expand-on-hover."));
    AddDescriptionPrefix(steps, "WaitForExpandOnHover()");
    return steps;
  }

  auto ExpandOnHover() {
    auto steps =
        Steps(MoveMouseTo(kTabStripRegionElementId), WaitForExpandOnHover());
    AddDescriptionPrefix(steps, "ExpandOnHover()");
    return steps;
  }

  auto WaitForPanelOpen() {
    auto steps = Steps(
        InParallel(RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                               kShowAnimationComplete)),
                   RunSubsequence(WaitForShow(kOrganizerPanelElementId))));
    AddDescriptionPrefix(steps, "WaitForPanelOpen()");
    return steps;
  }

  auto OpenOrganizerPanel() {
    auto steps =
        Steps(EnsureNotPresent(kOrganizerPanelElementId),
              PressButton(kTabSearchButtonElementId), WaitForPanelOpen());
    AddDescriptionPrefix(steps, "OpenOrganizerPanel()");
    return steps;
  }

  auto WaitForPanelClose() {
    return InParallel(RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                                  kHideAnimationComplete)),
                      RunSubsequence(WaitForHide(kOrganizerPanelElementId)));
  }

  auto WaitForPanelLoad() {
    auto steps =
        Steps(WaitForShow(OrganizerPanelView::kWebViewElementId),
              CheckView(OrganizerPanelView::kWebViewElementId,
                        [](views::View* view) {
                          return view->size() == view->parent()->size();
                        }),
              InstrumentNonTabWebView(kWebContentsId,
                                      OrganizerPanelView::kWebViewElementId),
              WaitForWebContentsReady(kWebContentsId,
                                      GURL(chrome::kChromeUIOrganizerPanelURL)),
              WaitForWebContentsPainted(kWebContentsId));
    AddDescriptionPrefix(steps, "WaitForPanelLoad()");
    return steps;
  }

  auto CloseOrganizerPanel() {
    auto steps =
        Steps(EnsurePresent(kOrganizerPanelElementId),
              If(
                  [this]() {
                    return OrganizerPanelController::From(browser())
                               ->GetCurrentOrganizerPanelLocation() ==
                           OrganizerPanelLocation::kOrganizerTray;
                  },
                  Then(PressButton(kOrganizerPanelCloseButtonElementId)),
                  Else(PressButton(kTabSearchButtonElementId))),
              WaitForPanelClose());
    AddDescriptionPrefix(steps, "CloseOrganizerPanel()");
    return steps;
  }

  auto ResizeVerticalTabsRegionToWidth(int width) {
    return Do([this, width]() {
      auto* vt_region_view =
          browser_view()->vertical_tab_strip_region_view_for_testing();
      vt_region_view->OnResize(width - vt_region_view->width(), true);
      RunScheduledLayouts();
    });
  }

  auto CheckControllerState(bool visible) {
    return CheckResult(
               [this]() {
                 return organizer_panel_controller()->IsOrganizerPanelVisible();
               },
               visible)
        .SetDescription("CheckControllerState");
  }

  auto CheckCurrentAnimation(BrowserAnimationMotion expected_motion) {
    return CheckResult(
        [this]() {
          return BrowserAnimationController::From(browser())->GetCurrentMotion(
              OrganizerPanelAnimations::kOrganizerPanel);
        },
        expected_motion, "CheckCurrentAnimation()");
  }

  auto CheckPanelVisuals(int expected_width, bool should_have_rounded_corners) {
    auto steps = Steps(
        CheckView(
            kOrganizerPanelElementId,
            [](OrganizerPanelView* panel_view) { return panel_view->width(); },
            expected_width)
            .SetDescription("Panel has expected width."),
        CheckView(
            kOrganizerPanelElementId,
            [](OrganizerPanelView* panel_view) { return panel_view->height(); },
            testing::Gt(0))
            .SetDescription("Panel has nonzero height."),
        CheckView(
            kOrganizerPanelElementId,
            [](OrganizerPanelView* panel_view) {
              const auto radii = panel_view->layer()->rounded_corner_radii();
              // Leading corners may be rounded to accommodate the window
              // itself, so only count the trailing corners.
              return radii.upper_right() > 0.0f || radii.lower_right() > 0.0f;
            },
            should_have_rounded_corners)
            .SetDescription("Panel has expected corners."));
    AddDescriptionPrefix(steps, "CheckPanelVisuals");
    return steps;
  }

  auto ExpectPanelLocation(OrganizerPanelLocation location) {
    return CheckResult(
        [this]() {
          return OrganizerPanelController::From(browser())
              ->GetCurrentOrganizerPanelLocation();
        },
        location, "ExpectPanelLocation()");
  }

  OrganizerPanelController* organizer_panel_controller() {
    return OrganizerPanelController::From(browser());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<base::CallbackListSubscription> animation_subscriptions_;
  std::unique_ptr<ExpandOnHoverLock> expand_on_hover_lock_;
};

// Horizontal tab strip state with tray.

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, OpenClosePanelHorizontalTabs) {
  RunTestSequence(SetVerticalTabsEnabled(false), CheckControllerState(false),
                  OpenOrganizerPanel(), CheckControllerState(true),
                  ExpectPanelLocation(OrganizerPanelLocation::kOrganizerTray),
                  CheckPanelVisuals(organizer_panel::kOrganizerPanelMinWidth,
                                    /*should_have_rounded_corners=*/true),
                  WaitForPanelLoad(), CloseOrganizerPanel(),
                  CheckControllerState(false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest,
                       OpenClosePanelHorizontalTabsUsingAccelerator) {
  ui::Accelerator tab_search_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_TAB_SEARCH, &tab_search_accelerator));

  RunTestSequence(
      SetVerticalTabsEnabled(false),
      SendAccelerator(kBrowserViewElementId, tab_search_accelerator),
      WaitForPanelOpen(),
      SendAccelerator(kBrowserViewElementId, tab_search_accelerator),
      WaitForPanelClose());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, TrayCloseOnClickOutside) {
  RunTestSequence(SetVerticalTabsEnabled(false), OpenOrganizerPanel(),
                  // Click on the Omnibox (outside the panel).
                  MoveMouseTo(kOmniboxElementId), ClickMouse(),
                  // This should close the tray.
                  WaitForPanelClose(), CheckControllerState(false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, TrayGrabsFocusOnOpen) {
  RunTestSequence(SetVerticalTabsEnabled(false), OpenOrganizerPanel(),
                  CheckViewProperty(OrganizerTrayView::kTrayElementId,
                                    &views::View::HasFocus, true));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, TrayClosesOnFocusLost) {
  RunTestSequence(SetVerticalTabsEnabled(false), OpenOrganizerPanel(),
                  // Focus the omnibox.
                  FocusElement(kOmniboxElementId), WaitForPanelClose(),
                  CheckControllerState(false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, TrayRestoresFocusOnClose) {
  RunTestSequence(
      SetVerticalTabsEnabled(false),
      // Focus the omnibox.
      FocusElement(kOmniboxElementId),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true),
      OpenOrganizerPanel(),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, false),
      CloseOrganizerPanel(),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, TrayDoesNotCloseOnClickInside) {
  RunTestSequence(SetVerticalTabsEnabled(false), OpenOrganizerPanel(),
                  MoveMouseTo(kOrganizerPanelElementId), ClickMouse(),
                  // Ensure controller doesn't think it's collapsing.
                  CheckControllerState(true),
                  CheckCurrentAnimation(BrowserAnimationMotion()),
                  EnsurePresent(OrganizerTrayView::kTrayElementId));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, TrayClosesOnEscPressed) {
  RunTestSequence(
      SetVerticalTabsEnabled(false), OpenOrganizerPanel(),
      SendAccelerator(kBrowserViewElementId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForPanelClose(), CheckControllerState(false));
}

// Vertical tab strip with tray or embedded organizer panel.

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest, OpenClosePanelVerticalTabs) {
  RunTestSequence(
      SetVerticalTabsEnabled(true), CheckControllerState(false),
      OpenOrganizerPanel(), CheckControllerState(true),
      ExpectPanelLocation(OrganizerPanelLocation::kVerticalTabStrip),
      CheckPanelVisuals(tabs::VerticalTabStripStateController::From(browser())
                            ->GetUncollapsedWidth(),
                        /*should_have_rounded_corners=*/false),
      WaitForPanelLoad(), CloseOrganizerPanel(), CheckControllerState(false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest,
                       OpenClosePanelVerticalTabsUsingAccelerator) {
  ui::Accelerator tab_search_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_TAB_SEARCH, &tab_search_accelerator));

  RunTestSequence(
      SetVerticalTabsEnabled(true),
      SendAccelerator(kBrowserViewElementId, tab_search_accelerator),
      WaitForPanelOpen(),
      SendAccelerator(kBrowserViewElementId, tab_search_accelerator),
      WaitForPanelClose());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest,
                       MatchesVerticalTabsWidthWhenLargerThanMinWidth) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth + 100;
  RunTestSequence(
      SetVerticalTabsEnabled(true),
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      OpenOrganizerPanel(),
      ExpectPanelLocation(OrganizerPanelLocation::kVerticalTabStrip),
      CheckPanelVisuals(kVerticalTabsRegionWidth,
                        /*should_have_rounded_corners=*/false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest,
                       MatchesVerticalTabsWidthAtMinAllowedEmbedWidth) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth -
      organizer_panel::kOrganizerPanelMinOverlap + 1;
  RunTestSequence(
      SetVerticalTabsEnabled(true),
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      OpenOrganizerPanel(),
      ExpectPanelLocation(OrganizerPanelLocation::kVerticalTabStrip),
      CheckPanelVisuals(kVerticalTabsRegionWidth,
                        /*should_have_rounded_corners=*/false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest,
                       AppearsInTrayWhenVerticalTabsBelowMinAllowedEmbedWidth) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth -
      organizer_panel::kOrganizerPanelMinOverlap;
  RunTestSequence(SetVerticalTabsEnabled(true),
                  ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
                  OpenOrganizerPanel(),
                  ExpectPanelLocation(OrganizerPanelLocation::kOrganizerTray),
                  CheckPanelVisuals(organizer_panel::kOrganizerPanelMinWidth,
                                    /*should_have_rounded_corners=*/true));
}

IN_PROC_BROWSER_TEST_F(
    OrganizerPanelUiTest,
    AppearsInTrayWhenVerticalTabsCollapsedAndNotExpandOnHover) {
  RunTestSequence(
      SetVerticalTabsEnabled(true, /*expand_on_hover_enabled=*/false),
      CollapseTabStrip(), OpenOrganizerPanel(),
      ExpectPanelLocation(OrganizerPanelLocation::kOrganizerTray),
      CheckPanelVisuals(organizer_panel::kOrganizerPanelMinWidth,
                        /*should_have_rounded_corners=*/true));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest,
                       TriggersExpandOnHoverWhenOpenedViaAccelerator) {
  ui::Accelerator tab_search_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_TAB_SEARCH, &tab_search_accelerator));

  RunTestSequence(
      SetVerticalTabsEnabled(true, /*expand_on_hover_enabled=*/true),
      CollapseTabStrip(),
      SendAccelerator(kBrowserViewElementId, tab_search_accelerator),
      WaitForExpandOnHover(),
      ExpectPanelLocation(OrganizerPanelLocation::kVerticalTabStrip),
      CheckPanelVisuals(tabs::kVerticalTabStripDefaultUncollapsedWidth, false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelUiTest,
                       AppearsInTabStripWhenVerticalTabsExpandOnHover) {
  RunTestSequence(
      SetVerticalTabsEnabled(true, /*expand_on_hover_enabled=*/true),
      CollapseTabStrip(), ExpandOnHover(), OpenOrganizerPanel(),
      ExpectPanelLocation(OrganizerPanelLocation::kVerticalTabStrip),
      CheckPanelVisuals(tabs::kVerticalTabStripDefaultUncollapsedWidth, false));
}

class OrganizerPanelAnimationUiTest : public OrganizerPanelUiTest {
 public:
  OrganizerPanelAnimationUiTest() = default;
  ~OrganizerPanelAnimationUiTest() override = default;

  void SetUpOnMainThread() override {
    OrganizerPanelUiTest::SetUpOnMainThread();

    auto container = base::MakeRefCounted<gfx::AnimationContainer>();
    BrowserAnimationController::From(browser())
        ->SetAnimationContainerForTesting(
            OrganizerPanelAnimations::kOrganizerPanel, container.get());
    animation_test_api_ =
        std::make_unique<gfx::AnimationContainerTestApi>(container.get());
  }

  void TearDownOnMainThread() override {
    animation_test_api_.reset();
    OrganizerPanelUiTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<gfx::AnimationContainerTestApi> animation_test_api_;
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelAnimationUiTest,
                       PanelSlidesAcrossVerticalTabStrip) {
  gfx::Rect region_bounds;
  gfx::Rect tab_strip_bounds;
  gfx::Rect organizer_panel_bounds;

  RunTestSequence(
      SetVerticalTabsEnabled(true), PressButton(kTabSearchButtonElementId),
      Do([this]() {
        animation_test_api_->IncrementTime(
            base::Milliseconds(browser_animations::kFlyoutShowMs / 2));
      }),
      WaitForShow(kOrganizerPanelElementId),
      WithElement(kTabStripRegionElementId,
                  [&](ui::TrackedElement* el) {
                    region_bounds = el->GetScreenBounds();
                  }),
      WithElement(kTabStripElementId,
                  [&](ui::TrackedElement* el) {
                    tab_strip_bounds = el->GetScreenBounds();
                  }),
      WithElement(kOrganizerPanelElementId,
                  [&](ui::TrackedElement* el) {
                    organizer_panel_bounds = el->GetScreenBounds();
                  }),
      Check([&]() { return !region_bounds.IsEmpty(); },
            "Region bounds not empty."),
      Check([&]() { return !tab_strip_bounds.IsEmpty(); },
            "Tab strip bounds not empty."),
      Check([&]() { return !organizer_panel_bounds.IsEmpty(); },
            "Organizer panel bounds not empty."),
      Check(
          [&]() {
            return organizer_panel_bounds.x() == tab_strip_bounds.right();
          },
          "Organizer and tab strip touch."),
      Check(
          [&]() {
            return organizer_panel_bounds.width() == region_bounds.width();
          },
          "Organizer and region have same width."),
      Check([&]() { return tab_strip_bounds.width() == region_bounds.width(); },
            "Tab strip and region have same width."),
      Check([&]() { return tab_strip_bounds.x() < region_bounds.x(); },
            "Tab strip is clipped on the left."),
      Check(
          [&]() {
            return organizer_panel_bounds.right() > region_bounds.right();
          },
          "Tab strip is clipped on the left."));
}

class OrganizerPanelPixelTest : public OrganizerPanelUiTest {
 public:
  OrganizerPanelPixelTest() = default;
  ~OrganizerPanelPixelTest() override = default;

  // Screenshots segments of the panel top and bottom to ensure visual
  // consistency. Remember to add any tests which use this to the file
  // `pixel_tests.filter`.
  auto ScreenshotPanel() {
    constexpr std::string kBaselineCL = "8451947";
    auto steps = Steps(
        WaitForPanelLoad(),
        SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                "Screenshot not supported on all platforms."),
        Screenshot(kBrowserViewElementId, "panel_top", kBaselineCL,
                   [this]() {
                     auto rect = GetPanelRectInBrowser();
                     rect.set_height(1);
                     rect.Outset(gfx::Outsets::TLBR(16, 8, 8, 8));
                     return rect;
                   }),
        Screenshot(kBrowserViewElementId, "panel_bottom", kBaselineCL,
                   [this]() {
                     auto rect = GetPanelRectInBrowser();
                     rect.set_y(rect.bottom() - 1);
                     rect.set_height(1);
                     rect.Outset(gfx::Outsets::TLBR(8, 8, 16, 8));
                     return rect;
                   }),
        SetOnIncompatibleAction(OnIncompatibleAction::kFailTest,
                                "Restoring default state."));

    AddDescriptionPrefix(steps, "MaybeScreenshotPanel()");
    return steps;
  }

 private:
  gfx::Rect GetPanelRectInBrowser() const {
    auto* const elements = BrowserElementsViews::From(browser());
    auto* const browser_view = elements->GetView(kBrowserViewElementId);
    auto* const panel_view = elements->GetView(kOrganizerPanelElementId);
    return views::View::ConvertRectToTarget(panel_view, browser_view,
                                            panel_view->GetLocalBounds());
  }
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelPixelTest, HorizontalTabs) {
  RunTestSequence(SetVerticalTabsEnabled(false), OpenOrganizerPanel(),
                  ScreenshotPanel());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPixelTest, VerticalTabsEmbedded) {
  RunTestSequence(SetVerticalTabsEnabled(true), OpenOrganizerPanel(),
                  ScreenshotPanel());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPixelTest, VerticalTabsWithTray) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth -
      organizer_panel::kOrganizerPanelMinOverlap;
  RunTestSequence(SetVerticalTabsEnabled(true),
                  ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
                  OpenOrganizerPanel(), ScreenshotPanel());
}
