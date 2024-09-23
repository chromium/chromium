// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Tests of window placement for popup browser windows.
using PopupTest = PopupTestBase;

// A helper class to wait for the bounds of two widgets to become equal.
class WidgetBoundsEqualWaiter final : public views::WidgetObserver {
 public:
  WidgetBoundsEqualWaiter(views::Widget* widget, views::Widget* widget_cmp)
      : widget_(widget), widget_cmp_(widget_cmp) {
    widget_->AddObserver(this);
    widget_cmp_->AddObserver(this);
  }

  WidgetBoundsEqualWaiter(const WidgetBoundsEqualWaiter&) = delete;
  WidgetBoundsEqualWaiter& operator=(const WidgetBoundsEqualWaiter&) = delete;
  ~WidgetBoundsEqualWaiter() final {
    widget_->RemoveObserver(this);
    widget_cmp_->RemoveObserver(this);
  }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& rect) final {
    if (WidgetsBoundsEqual()) {
      widget_->RemoveObserver(this);
      widget_cmp_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  // Wait for changes to occur, or return immediately if they already have.
  void Wait() {
    if (!WidgetsBoundsEqual()) {
      run_loop_.Run();
    }
  }

 private:
  bool WidgetsBoundsEqual() {
    return widget_->GetWindowBoundsInScreen() ==
           widget_cmp_->GetWindowBoundsInScreen();
  }
  const raw_ptr<views::Widget> widget_ = nullptr;
  const raw_ptr<views::Widget> widget_cmp_ = nullptr;
  base::RunLoop run_loop_;
};

// Ensure `left=0,top=0` popup window feature coordinates are respected.
IN_PROC_BROWSER_TEST_F(PopupTest, OpenLeftAndTopZeroCoordinates) {
  // Attempt to open a popup at (0,0). Its bounds should match the request, but
  // be adjusted to meet minimum size and available display area constraints.
  Browser* popup =
      OpenPopup(browser(), "open('.', '', 'left=0,top=0,width=50,height=50')");
  const display::Display display = GetDisplayNearestBrowser(popup);
  gfx::Rect expected(popup->window()->GetBounds().size());
  expected.AdjustToFit(display.work_area());
#if BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/40815883) Desktop Linux window bounds are inaccurate.
  expected.Outset(50);
  EXPECT_TRUE(expected.Contains(popup->window()->GetBounds()))
      << " expected: " << expected.ToString()
      << " popup: " << popup->window()->GetBounds().ToString()
      << " work_area: " << display.work_area().ToString();
#else
  EXPECT_EQ(expected.ToString(), popup->window()->GetBounds().ToString())
      << " work_area: " << display.work_area().ToString();
#endif
}

// Ensure popups are opened in the available space of the opener's display.
IN_PROC_BROWSER_TEST_F(PopupTest, OpenClampedToCurrentDisplay) {
  // Attempt to open popups outside the bounds of the opener's display.
  const char* const open_features[] = {
      ("left=${screen.availLeft-50},top=${screen.availTop-50}"
       ",width=200,height=200"),
      ("left=${screen.availLeft+screen.availWidth+50}"
       ",top=${screen.availTop+screen.availHeight+50},width=200,height=200"),
      ("left=${screen.availLeft+screen.availWidth-50}"
       ",top=${screen.availTop+screen.availHeight-50},width=500,height=500,"),
      "width=${screen.availWidth+300},height=${screen.availHeight+300}",
  };
  const display::Display display = GetDisplayNearestBrowser(browser());
  for (const char* const features : open_features) {
    const std::string script = "open('.', '', `" + std::string(features) + "`)";
    Browser* popup = OpenPopup(browser(), script);
    // The popup should be constrained to the opener's available display space.
    EXPECT_EQ(display, GetDisplayNearestBrowser(popup));
    gfx::Rect work_area(display.work_area());
#if BUILDFLAG(IS_LINUX)
    // TODO(crbug.com/40815883) Desktop Linux bounds flakily extend outside the
    // work area on trybots, when opening with excessive width and height, e.g.:
    // width=${screen.availWidth+300},height=${screen.availHeight+300} yields:
    // work_area: 0,0 1280x800 popup: 1,20 1280x800
    work_area.Outset(50);
#endif
    EXPECT_TRUE(work_area.Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup->window()->GetBounds().ToString();
  }
}

// Ensure popups cannot be moved beyond the available display space by script.
IN_PROC_BROWSER_TEST_F(PopupTest, MoveClampedToCurrentDisplay) {
  const char kOpenPopup[] =
      ("open('.', '', `left=${screen.availLeft+screen.availWidth/2}"
       ",top=${screen.availTop+screen.availHeight/2},width=200,height=200`)");
  const char* const kMoveScripts[] = {
      "moveBy(screen.availWidth*2, screen.availHeight* 2)",
      "moveBy(screen.availWidth*-2, screen.availHeight*-2)",
      ("moveTo(screen.availLeft+screen.availWidth+50,"
       "screen.availTop+screen.availHeight+50)"),
      "moveTo(screen.availLeft-50, screen.availTop-50)",
  };
  const display::Display display = GetDisplayNearestBrowser(browser());
  for (const char* const script : kMoveScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    gfx::Rect popup_bounds = popup->window()->GetBounds();
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();
    SCOPED_TRACE(testing::Message()
                 << " script: " << script
                 << " work_area: " << display.work_area().ToString()
                 << " popup-before: " << popup_bounds.ToString());
    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for a substantial move, bounds change during init.
    WaitForBoundsChange(popup, /*move_by=*/40, /*resize_by=*/0);
    EXPECT_NE(popup_bounds.origin(), popup->window()->GetBounds().origin());
    EXPECT_EQ(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " popup-after: " << popup->window()->GetBounds().ToString();
  }
}

// Ensure popups cannot be resized beyond the available display space by script.
IN_PROC_BROWSER_TEST_F(PopupTest, ResizeClampedToCurrentDisplay) {
  const char kOpenPopup[] =
      ("open('.', '', `left=${screen.availLeft},top=${screen.availTop}"
       ",width=200,height=200`)");
  const char* const kResizeScripts[] = {
      "resizeBy(screen.availWidth*2, screen.availHeight*2)",
      "resizeTo(screen.availWidth+200, screen.availHeight+200)",
  };
  const display::Display display = GetDisplayNearestBrowser(browser());
  for (const char* const script : kResizeScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    gfx::Rect popup_bounds = popup->window()->GetBounds();
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();
    SCOPED_TRACE(testing::Message()
                 << " script: " << script
                 << " work_area: " << display.work_area().ToString()
                 << " popup-before: " << popup_bounds.ToString());
    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for a substantial resize, bounds change during init.
    WaitForBoundsChange(popup, /*move_by=*/0, /*resize_by=*/99);
    EXPECT_NE(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " popup-after: " << popup->window()->GetBounds().ToString();
  }
}

// Opens two popups with custom position and size, but one has noopener. They
// should both have the same position and size. http://crbug.com/1011688
IN_PROC_BROWSER_TEST_F(PopupTest, NoopenerPositioning) {
  const char kFeatures[] =
      "left=${screen.availLeft},top=${screen.availTop},width=200,height=200";
  Browser* noopener_popup = OpenPopup(
      browser(), "open('.', '', `noopener=1," + std::string(kFeatures) + "`)");
  Browser* opener_popup =
      OpenPopup(browser(), "open('.', '', `" + std::string(kFeatures) + "`)");

  WidgetBoundsEqualWaiter(views::Widget::GetWidgetForNativeWindow(
                              noopener_popup->window()->GetNativeWindow()),
                          views::Widget::GetWidgetForNativeWindow(
                              opener_popup->window()->GetNativeWindow()))
      .Wait();

  EXPECT_EQ(noopener_popup->window()->GetBounds().ToString(),
            opener_popup->window()->GetBounds().ToString());
}

// Tests for Additional Windowing Controls on popup windows.
// https://chromestatus.com/feature/5201832664629248
// For PWA tests see WebAppFrameToolbarBrowserTest_AdditionalWindowingControls
class PopupTest_AdditionalWindowingControls : public PopupTest {
 private:
  base::test::ScopedFeatureList feature_list{
      blink::features::kDesktopPWAsAdditionalWindowingControls};
};

// Ensure that moving a popup by moveTo/moveBy generates a `move` event.
// Note: window.moveTo/moveBy API is enabled only for popups and web apps.
IN_PROC_BROWSER_TEST_F(PopupTest_AdditionalWindowingControls,
                       MoveCallFiresMoveEvent) {
  const char popup_script[] =
      R"(var command = "%s";
      var coordString = (x, y) => `(X: ${x}, Y: ${y})`;
      new Promise((resolve, reject) => {
        const coord_before = coordString(screenX, screenY);
        addEventListener('move', e => resolve(`move fired`));
        setTimeout(() => {
          const coord_after = coordString(screenX, screenY);
          reject(`move not fired by ${command}; window position: `
               + `${coord_before} -> ${coord_after}`); }, 1000);
        %s;
        }).finally(()=>close()); )";

  for (const char* const move_command : {"moveBy(10, 10)", "moveTo(50, 50)"}) {
    std::string script =
        base::StringPrintf(popup_script, move_command, move_command);

    Browser* popup = OpenPopup(
        browser(), "open('.', '', 'left=0,top=0,width=50,height=50')");
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();

    gfx::Rect bounds_before = popup->window()->GetBounds();
    SCOPED_TRACE(testing::Message()
                 << " move-command: " << move_command
                 << " popup-before: " << bounds_before.ToString());
    EXPECT_EQ(content::EvalJs(popup_contents, script), "move fired");
    gfx::Rect bounds_after = popup->window()->GetBounds();
    EXPECT_NE(bounds_before.ToString(), bounds_after.ToString());
  }
}

}  // namespace
