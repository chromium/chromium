// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace zoom {
namespace {

class ZoomViewInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ZoomViewInteractiveUiTest() = default;

  ZoomViewInteractiveUiTest(const ZoomViewInteractiveUiTest&) = delete;
  ZoomViewInteractiveUiTest& operator=(const ZoomViewInteractiveUiTest&) =
      delete;

  ~ZoomViewInteractiveUiTest() override = default;

 protected:
  int GetZoomPercent() {
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();

    return ZoomController::FromWebContents(web_contents)->GetZoomPercent();
  }

  void SetZoomLevel(content::PageZoom zoom_level) {
    chrome::Zoom(browser(), zoom_level);
  }

  auto DoZoomIn() {
    return Do([&]() { SetZoomLevel(content::PAGE_ZOOM_IN); });
  }

  auto DoZoomOut() {
    return Do([&]() { SetZoomLevel(content::PAGE_ZOOM_OUT); });
  }

  auto DoZoomReset() {
    return Do([&]() { SetZoomLevel(content::PAGE_ZOOM_RESET); });
  }

  auto WaitForZoomBubbleShow() { return WaitForZoomBubble(/*visible=*/true); }

  auto WaitForZoomBubbleHide() { return WaitForZoomBubble(/*visible=*/false); }

  MultiStep WaitForZoomBubble(bool visible) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kZoomBubbleVisible);

    return Steps(
        PollState(kZoomBubbleVisible,
                  [&, visible]() {
                    const bool is_visible =
                        ZoomBubbleCoordinator::From(browser())->bubble() !=
                        nullptr;
                    return is_visible == visible;
                  }),
        WaitForState(kZoomBubbleVisible, true),
        StopObservingState(kZoomBubbleVisible));
  }

  auto ZoomAccessor() {
    return page_actions::PageActionTestAccessor(browser(),
                                                kActionShowZoomBubble);
  }

  auto CheckZoomToolTip(std::u16string_view expected) {
    return CheckResult([&]() { return ZoomAccessor().GetTooltipText(); },
                       expected);
  }

  auto CheckZoomAccessibleName(std::u16string_view expected) {
    return CheckResult([&]() { return ZoomAccessor().GetAccessibleName(); },
                       expected);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest, ZoomStateUpdates) {
  // We'll store the zoom-in image so we can compare it later to the zoom-out
  // image.
  ui::ImageModel zoom_in_image;
  RunTestSequence(
      WaitForZoomBubbleHide(), DoZoomIn(),
      WaitForShow(kActionItemZoomElementId), CheckZoomToolTip(u"Zoom: 110%"),
      CheckZoomAccessibleName(u"Zoom: 110%"),
      CheckResult([&]() { return GetZoomPercent(); }, testing::Eq(110)),
      Do([&]() { zoom_in_image = ZoomAccessor().GetImage(); }),
      WaitForZoomBubbleShow(), DoZoomReset(), WaitForZoomBubbleShow(),
      CheckResult([&]() { return GetZoomPercent(); }, testing::Eq(100)),
      DoZoomOut(), WaitForShow(kActionItemZoomElementId),
      CheckZoomToolTip(u"Zoom: 90%"), CheckZoomAccessibleName(u"Zoom: 90%"),
      CheckResult([&]() { return GetZoomPercent(); }, testing::Eq(90)),
      Check([&]() { return ZoomAccessor().GetImage() != zoom_in_image; }),
      WaitForZoomBubbleShow());
}

IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       ShowAndHideZoomBubbleByClickWithMouse) {
  RunTestSequence(
      Do([&]() {
        // Disable click suppression so this can be tested without sleeps.
        ZoomAccessor().SetSuppressionThreshold(base::TimeDelta());
      }),
      WaitForZoomBubbleHide(), DoZoomIn(),
      WaitForShow(kActionItemZoomElementId),
      MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
      WaitForZoomBubbleShow(), MoveMouseTo(kActionItemZoomElementId),
      ClickMouse(), WaitForZoomBubbleHide(),
      CheckResult(
          [&]() { return ZoomBubbleCoordinator::From(browser())->IsShowing(); },
          false),
      MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
      WaitForZoomBubbleShow());
}

IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       ShowAndHideZoomBubbleByClickWithKeyboardPress) {
  RunTestSequence(WaitForZoomBubbleHide(), DoZoomIn(),
                  WaitForShow(kActionItemZoomElementId),
                  PressButton(kActionItemZoomElementId, InputType::kKeyboard),
                  WaitForZoomBubbleShow(),
                  PressButton(kActionItemZoomElementId, InputType::kKeyboard),
                  WaitForZoomBubbleHide(),
                  PressButton(kActionItemZoomElementId, InputType::kKeyboard),
                  WaitForZoomBubbleShow());
}

// Verifies that after a "Reset", and then after the closing of the
// bubble, the page-action icon disappears immediately.
IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       IconHidesAfterResetAndBubbleClose) {
  RunTestSequence(
      WaitForZoomBubbleHide(), DoZoomIn(),
      WaitForShow(kActionItemZoomElementId),
      MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
      WaitForZoomBubbleShow(), DoZoomReset(),
      CheckResult([&] { return GetZoomPercent(); }, testing::Eq(100)),
      WaitForZoomBubbleShow(), MoveMouseTo(kActionItemZoomElementId),
      ClickMouse(), WaitForZoomBubbleHide(),
      WaitForHide(kActionItemZoomElementId));
}

// Verifies that if the user resets to 100 % *without* opening the bubble
// first, the icon is hidden right after the reset, independent of the
// bubble’s existence.
IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       IconHidesImmediatelyAfterProgrammaticReset) {
  RunTestSequence(WaitForZoomBubbleHide(), DoZoomIn(),
                  WaitForShow(kActionItemZoomElementId), DoZoomReset(),
                  WaitForHide(kActionItemZoomElementId),
                  WaitForZoomBubbleHide());
}

// Verifies that the icon remains hidden after an auto-closed bubble at
// default zoom.
IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       IconStaysHiddenAfterAutoCloseAtDefaultZoom) {
  RunTestSequence(WaitForZoomBubbleHide(), DoZoomIn(),
                  MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
                  WaitForZoomBubbleShow(), DoZoomReset(),
                  CheckResult([&] { return GetZoomPercent(); }, 100),
                  WaitForZoomBubbleHide(),
                  WaitForHide(kActionItemZoomElementId));
}

IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       AccessibleNameUpdatesWhileBubbleVisible) {
  RunTestSequence(
      WaitForZoomBubbleHide(), DoZoomIn(),
      WaitForShow(kActionItemZoomElementId),
      CheckResult([&]() { return GetZoomPercent(); }, testing::Eq(110)),
      CheckZoomAccessibleName(u"Zoom: 110%"),
      MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
      WaitForZoomBubbleShow(), DoZoomIn(),
      CheckZoomAccessibleName(u"Zoom: 125%"), WaitForZoomBubbleShow());
}

}  // namespace
}  // namespace zoom
