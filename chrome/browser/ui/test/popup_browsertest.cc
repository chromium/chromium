// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/base_window.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
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
  BrowserWindowInterface* popup =
      OpenPopup(browser(), "open('.', '', 'left=0,top=0,width=50,height=50')");
  const display::Display display = GetDisplayNearestBrowser(popup);
  gfx::Rect expected(popup->GetWindow()->GetBounds().size());
  expected.AdjustToFit(display.work_area());
#if BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/40815883) Desktop Linux window bounds are inaccurate.
  expected.Outset(50);
  EXPECT_TRUE(expected.Contains(popup->GetWindow()->GetBounds()))
      << " expected: " << expected.ToString()
      << " popup: " << popup->GetWindow()->GetBounds().ToString()
      << " work_area: " << display.work_area().ToString();
#else
  EXPECT_EQ(expected.ToString(), popup->GetWindow()->GetBounds().ToString())
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
    BrowserWindowInterface* popup = OpenPopup(browser(), script);
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
    EXPECT_TRUE(work_area.Contains(popup->GetWindow()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup->GetWindow()->GetBounds().ToString();
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
    BrowserWindowInterface* popup = OpenPopup(browser(), kOpenPopup);
    gfx::Rect popup_bounds = popup->GetWindow()->GetBounds();
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();
    SCOPED_TRACE(testing::Message()
                 << " script: " << script
                 << " work_area: " << display.work_area().ToString()
                 << " popup-before: " << popup_bounds.ToString());
    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for a substantial move, bounds change during init.
    WaitForBoundsChange(popup, /*move_by=*/40, /*resize_by=*/0);
    EXPECT_NE(popup_bounds.origin(), popup->GetWindow()->GetBounds().origin());
    EXPECT_EQ(popup_bounds.size(), popup->GetWindow()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->GetWindow()->GetBounds()))
        << " popup-after: " << popup->GetWindow()->GetBounds().ToString();
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
    BrowserWindowInterface* popup = OpenPopup(browser(), kOpenPopup);
    gfx::Rect popup_bounds = popup->GetWindow()->GetBounds();
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();
    SCOPED_TRACE(testing::Message()
                 << " script: " << script
                 << " work_area: " << display.work_area().ToString()
                 << " popup-before: " << popup_bounds.ToString());
    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for a substantial resize, bounds change during init.
    WaitForBoundsChange(popup, /*move_by=*/0, /*resize_by=*/99);
    EXPECT_NE(popup_bounds.size(), popup->GetWindow()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->GetWindow()->GetBounds()))
        << " popup-after: " << popup->GetWindow()->GetBounds().ToString();
  }
}

// Opens two popups with custom position and size, but one has noopener. They
// should both have the same position and size. http://crbug.com/40651776
IN_PROC_BROWSER_TEST_F(PopupTest, NoopenerPositioning) {
  const char kFeatures[] =
      "left=${screen.availLeft},top=${screen.availTop},width=200,height=200";
  BrowserWindowInterface* noopener_popup = OpenPopup(
      browser(), "open('.', '', `noopener=1," + std::string(kFeatures) + "`)");
  BrowserWindowInterface* opener_popup =
      OpenPopup(browser(), "open('.', '', `" + std::string(kFeatures) + "`)");

  WidgetBoundsEqualWaiter(views::Widget::GetWidgetForNativeWindow(
                              noopener_popup->GetWindow()->GetNativeWindow()),
                          views::Widget::GetWidgetForNativeWindow(
                              opener_popup->GetWindow()->GetNativeWindow()))
      .Wait();

  EXPECT_EQ(noopener_popup->GetWindow()->GetBounds().ToString(),
            opener_popup->GetWindow()->GetBounds().ToString());
}

// Regression test for https://crbug.com/512533947: a popup whose document
// calls window.moveTo() during page load must not shrink on reload.
IN_PROC_BROWSER_TEST_F(PopupTest, MoveToOnReloadDoesNotShrinkOuterBounds) {
  BrowserWindowInterface* popup = OpenPopup(
      browser(), "open('.', '', 'left=200,top=200,width=600,height=400')");
  ASSERT_TRUE(popup);
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();

  // Browser-initiated navigation to a data: URL whose inline script calls
  // moveTo(0, 0) during parse, mirroring the reporter's popup.html.
  constexpr char kPopupUrl[] =
      "data:text/html,<script>window.moveTo(0,0)</script>hi";
  ASSERT_TRUE(content::NavigateToURL(popup_contents, GURL(kPopupUrl)));
  // The inline moveTo(0, 0) settles the popup near the screen origin.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return popup->GetWindow()->GetBounds().x() < 100; }));
  const gfx::Size initial_size = popup->GetWindow()->GetBounds().size();

  // The bug is a race between the reload commit and the inline moveTo(0, 0),
  // so it only reproduces intermittently. Repeat the cycle a few times to
  // reliably surface it.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "reload iteration: " << i);
    // Shift the popup off the screen origin so the upcoming inline
    // moveTo(0, 0) is observable as an origin change rather than a no-op.
    const gfx::Rect shifted_bounds(200, 200, initial_size.width(),
                                   initial_size.height());
    popup->GetWindow()->SetBounds(shifted_bounds);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return popup->GetWindow()->GetBounds().x() >= 100; }));

    popup_contents->GetController().Reload(content::ReloadType::NORMAL,
                                           /*check_for_repost=*/false);
    EXPECT_TRUE(content::WaitForLoadStop(popup_contents));
    // Wait for the post-reload inline moveTo(0, 0) to be applied by the
    // browser; this is observable as the popup origin returning near zero.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return popup->GetWindow()->GetBounds().x() < 100; }));

    const gfx::Rect bounds_after = popup->GetWindow()->GetBounds();
    EXPECT_EQ(initial_size, bounds_after.size())
        << " initial-size: " << initial_size.ToString()
        << " bounds-after: " << bounds_after.ToString();
  }
}

// Parallel regression test for the resize side of crbug.com/512533947: a
// popup whose document calls window.resizeTo() during page load must not
// silently shift the outer origin on reload.
IN_PROC_BROWSER_TEST_F(PopupTest, ResizeToOnReloadDoesNotShiftOuterBounds) {
  BrowserWindowInterface* popup = OpenPopup(
      browser(), "open('.', '', 'left=200,top=200,width=600,height=400')");
  ASSERT_TRUE(popup);
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();

  constexpr char kPopupUrl[] =
      "data:text/html,<script>window.resizeTo(500,300)</script>hi";
  ASSERT_TRUE(content::NavigateToURL(popup_contents, GURL(kPopupUrl)));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return popup->GetWindow()->GetBounds().width() <= 500; }));
  const gfx::Point initial_origin = popup->GetWindow()->GetBounds().origin();
  const gfx::Size initial_size = popup->GetWindow()->GetBounds().size();

  // The bug is a race between the reload commit and the inline
  // resizeTo(500,300), so it only reproduces intermittently. Repeat the cycle
  // a few times to reliably surface it.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "reload iteration: " << i);
    // Stretch the popup so the upcoming inline resizeTo(500,300) is
    // observable as a size shrink rather than a no-op.
    const gfx::Rect stretched_bounds(initial_origin,
                                     gfx::Size(700, initial_size.height()));
    popup->GetWindow()->SetBounds(stretched_bounds);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return popup->GetWindow()->GetBounds().width() >= 700; }));

    popup_contents->GetController().Reload(content::ReloadType::NORMAL,
                                           /*check_for_repost=*/false);
    EXPECT_TRUE(content::WaitForLoadStop(popup_contents));
    // Wait for the post-reload inline resizeTo(500,300) to be applied by
    // the browser; observable as the popup width returning under 700.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return popup->GetWindow()->GetBounds().width() < 700; }));

    const gfx::Rect bounds_after = popup->GetWindow()->GetBounds();
    EXPECT_EQ(initial_origin, bounds_after.origin())
        << " initial-origin: " << initial_origin.ToString()
        << " bounds-after: " << bounds_after.ToString();
  }
}

// TODO(crbug.com/512533947): Add bad-message browser tests verifying that
// MoveWindowTo / ResizeWindowTo / SetWindowRect from a prerendered page or
// from a subframe trigger ReportBadMessage via
// RenderFrameHostImpl::ValidateOutermostMainFrameWindowChange. These tests
// belong next to existing site_per_process_browsertest.cc bad-message
// fixtures and require a forged subframe LocalMainFrameHost binding.

// Tests for Additional Windowing Controls on popup windows.
// https://chromestatus.com/feature/5201832664629248
// For PWA tests see WebAppFrameToolbarBrowserTest_AdditionalWindowingControls
class PopupTest_AdditionalWindowingControls : public PopupTest {
 private:
  base::test::ScopedFeatureList feature_list{
      blink::features::kDesktopPWAsAdditionalWindowingControlsOnMove};
};

// Ensure that moving a popup by moveTo/moveBy generates a `move` event.
// Note: window.moveTo/moveBy API is enabled only for popups and web apps.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_MoveCallFiresMoveEvent DISABLED_MoveCallFiresMoveEvent
#else
#define MAYBE_MoveCallFiresMoveEvent MoveCallFiresMoveEvent
#endif
IN_PROC_BROWSER_TEST_F(PopupTest_AdditionalWindowingControls,
                       MAYBE_MoveCallFiresMoveEvent) {
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

    BrowserWindowInterface* popup = OpenPopup(
        browser(), "open('.', '', 'left=0,top=0,width=50,height=50')");
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();

    gfx::Rect bounds_before = popup->GetWindow()->GetBounds();
    SCOPED_TRACE(testing::Message()
                 << " move-command: " << move_command
                 << " popup-before: " << bounds_before.ToString());
    EXPECT_EQ(content::EvalJs(popup_contents, script), "move fired");
    gfx::Rect bounds_after = popup->GetWindow()->GetBounds();
    EXPECT_NE(bounds_before.ToString(), bounds_after.ToString());
  }
}

}  // namespace
