// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/eye_dropper/eye_dropper_view.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"

class EyeDropperViewAuraInteractiveTest : public InProcessBrowserTest {
 public:
  EyeDropperViewAuraInteractiveTest() = default;

  class EyeDropperListener : public content::EyeDropperListener {
   public:
    void ColorSelected(SkColor color) override {}
    void ColorSelectionCanceled() override { is_canceled_ = true; }
    bool IsCanceled() const { return is_canceled_; }

   private:
    bool is_canceled_ = false;
  };
};

IN_PROC_BROWSER_TEST_F(EyeDropperViewAuraInteractiveTest, ActiveChangeCancel) {
  EyeDropperListener listener;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Focus();
  ASSERT_TRUE(web_contents->GetPrimaryMainFrame()->GetView()->HasFocus());
  std::unique_ptr<content::EyeDropper> eye_dropper =
      ShowEyeDropper(web_contents->GetPrimaryMainFrame(), &listener);
  ASSERT_TRUE(eye_dropper);
  EXPECT_FALSE(listener.IsCanceled());
  web_contents->GetRenderWidgetHostView()->Hide();
  EXPECT_TRUE(listener.IsCanceled());
}

IN_PROC_BROWSER_TEST_F(EyeDropperViewAuraInteractiveTest, InactiveWindow) {
  EyeDropperListener listener;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetRenderWidgetHostView()->Hide();
  ASSERT_FALSE(web_contents->GetPrimaryMainFrame()->GetView()->HasFocus());
  std::unique_ptr<content::EyeDropper> eye_dropper =
      ShowEyeDropper(web_contents->GetPrimaryMainFrame(), &listener);
  EXPECT_EQ(eye_dropper, nullptr);
}

IN_PROC_BROWSER_TEST_F(EyeDropperViewAuraInteractiveTest, MoveMouseAndTouch) {
  EyeDropperListener listener;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // EyeDropper should open at cursor.
  web_contents->Focus();
  ASSERT_TRUE(web_contents->GetPrimaryMainFrame()->GetView()->HasFocus());
  std::unique_ptr<content::EyeDropper> eye_dropper =
      ShowEyeDropper(web_contents->GetPrimaryMainFrame(), &listener);
  ASSERT_TRUE(eye_dropper);
  auto* view = static_cast<eye_dropper::EyeDropperView*>(eye_dropper.get());
  EXPECT_EQ(display::Screen::GetScreen()->GetCursorScreenPoint(),
            view->GetWidget()->GetWindowBoundsInScreen().CenterPoint());

  // EyeDropper should move as cursor moves.
  constexpr gfx::Point kCursorPosition1(101, 102);
  view->OnCursorPositionUpdate(kCursorPosition1);
  EXPECT_EQ(kCursorPosition1,
            view->GetWidget()->GetWindowBoundsInScreen().CenterPoint());

  // EyeDropper should move on touch events.
  constexpr gfx::Point kTouchStart(100, 100);
  const ui::PointerDetails kDetails(ui::EventPointerType::kTouch, 1);
  ui::TouchEvent press(ui::EventType::kTouchPressed, kTouchStart,
                       base::TimeTicks::Now(), kDetails);
  view->GetEventHandlerForTesting()->OnTouchEvent(&press);
  constexpr gfx::Point kTouchEnd(110, 110);
  ui::TouchEvent move(ui::EventType::kTouchMoved, kTouchEnd,
                      base::TimeTicks::Now(), kDetails);
  view->GetEventHandlerForTesting()->OnTouchEvent(&move);

  gfx::Vector2d kTouchMoved(kTouchEnd - kTouchStart);
  EXPECT_EQ(kCursorPosition1 + kTouchMoved,
            view->GetWidget()->GetWindowBoundsInScreen().CenterPoint());

  // EyeDropper should not move back to cursor until it moves.
  view->OnCursorPositionUpdate(kCursorPosition1);
  EXPECT_EQ(kCursorPosition1 + kTouchMoved,
            view->GetWidget()->GetWindowBoundsInScreen().CenterPoint());
  constexpr gfx::Point kCursorPosition2(122, 133);
  view->OnCursorPositionUpdate(kCursorPosition2);
  EXPECT_EQ(kCursorPosition2,
            view->GetWidget()->GetWindowBoundsInScreen().CenterPoint());
}
