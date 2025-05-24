// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/test/interaction/interactive_browser_test.h"
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
  ZoomViewInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationZoom.name, "true"}});
  }

  ZoomViewInteractiveUiTest(const ZoomViewInteractiveUiTest&) = delete;
  ZoomViewInteractiveUiTest& operator=(const ZoomViewInteractiveUiTest&) =
      delete;

  ~ZoomViewInteractiveUiTest() override = default;

 protected:
  int GetZoomPercent() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

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

    return Steps(PollState(kZoomBubbleVisible,
                           [&, visible]() {
                             bool is_visible =
                                 ZoomBubbleView::GetZoomBubble() != nullptr;
                             return is_visible == visible;
                           }),
                 WaitForState(kZoomBubbleVisible, true),
                 StopObservingState(kZoomBubbleVisible));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest, ZoomStateUpdates) {
  // We'll store the zoom-in image so we can compare it later to the zoom-out
  // image.
  ui::ImageModel zoom_in_image;
  RunTestSequence(
      WaitForZoomBubbleHide(), DoZoomIn(),
      WaitForShow(kActionItemZoomElementId),
      CheckViewProperty(kActionItemZoomElementId,
                        &page_actions::PageActionView::GetTooltipText,
                        u"Zoom: 110%"),
      CheckResult([&]() { return GetZoomPercent(); }, testing::Eq(110)),
      WithView(kActionItemZoomElementId,
               [&](page_actions::PageActionView* page_action_view) {
                 zoom_in_image =
                     page_action_view
                         ->GetImageModel(views::Button::STATE_NORMAL)
                         .value();
               }),
      WaitForZoomBubbleShow(), DoZoomReset(), WaitForZoomBubbleShow(),
      CheckResult([&]() { return GetZoomPercent(); }, testing::Eq(100)),
      DoZoomOut(), WaitForShow(kActionItemZoomElementId),
      CheckViewProperty(kActionItemZoomElementId,
                        &page_actions::PageActionView::GetTooltipText,
                        u"Zoom: 90%"),
      CheckResult([&]() { return GetZoomPercent(); }, testing::Eq(90)),
      CheckView(kActionItemZoomElementId,
                [&](page_actions::PageActionView* page_action_view) {
                  return page_action_view
                             ->GetImageModel(views::Button::STATE_NORMAL)
                             .value() != zoom_in_image;
                }),
      WaitForZoomBubbleShow());
}

IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       ShowAndHideZoomBubbleByClickWithMouse) {
  RunTestSequence(WaitForZoomBubbleHide(), DoZoomIn(),
                  WaitForShow(kActionItemZoomElementId),
                  MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
                  WaitForZoomBubbleShow(),
                  MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
                  WaitForZoomBubbleHide(),
                  MoveMouseTo(kActionItemZoomElementId), ClickMouse(),
                  WaitForZoomBubbleShow());
}

IN_PROC_BROWSER_TEST_F(ZoomViewInteractiveUiTest,
                       ShowAndHideZoomBubbleByClickWithKeyboardPress) {
  RunTestSequence(
      WaitForZoomBubbleHide(), DoZoomIn(),
      WaitForShow(kActionItemZoomElementId),
      PressButton(kActionItemZoomElementId), WaitForZoomBubbleShow(),
      PressButton(kActionItemZoomElementId), WaitForZoomBubbleHide(),
      PressButton(kActionItemZoomElementId), WaitForZoomBubbleShow());
}

}  // namespace
}  // namespace zoom
