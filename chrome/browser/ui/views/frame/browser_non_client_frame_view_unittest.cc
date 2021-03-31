// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/test/widget_test.h"
#include "url/gurl.h"

class BrowserNonClientFrameViewTest : public TestWithBrowserView {
 public:
  explicit BrowserNonClientFrameViewTest(Browser::Type type)
      : TestWithBrowserView(type), frame_view_(nullptr) {}

  // TestWithBrowserView override:
  void SetUp() override {
#if defined(OS_WIN)
    // Use opaque frame.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableDwmComposition);
#endif
    TestWithBrowserView::SetUp();
    views::Widget* widget = browser_view()->GetWidget();
    frame_view_ = static_cast<BrowserNonClientFrameView*>(
        widget->non_client_view()->frame_view());
  }

 protected:
  // Owned by the browser view.
  BrowserNonClientFrameView* frame_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewTest);
};

class BrowserNonClientFrameViewPopupTest
    : public BrowserNonClientFrameViewTest {
 public:
  BrowserNonClientFrameViewPopupTest()
      : BrowserNonClientFrameViewTest(Browser::TYPE_POPUP) {}
};

// TODO(crbug.com/998369): Flaky on Linux TSAN and ASAN.
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && \
    (defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER))
#define MAYBE_HitTestPopupTopChrome DISABLED_HitTestPopupTopChrome
#else
#define MAYBE_HitTestPopupTopChrome HitTestPopupTopChrome
#endif
TEST_F(BrowserNonClientFrameViewPopupTest, MAYBE_HitTestPopupTopChrome) {
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(-1, 4, 1, 1)));
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(4, -1, 1, 1)));
  const int top_inset = frame_view_->GetTopInset(false);
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(4, top_inset, 1, 1)));
  if (top_inset > 0)
    EXPECT_TRUE(frame_view_->HitTestRect(gfx::Rect(4, top_inset - 1, 1, 1)));
}

class BrowserNonClientFrameViewTabbedTest
    : public BrowserNonClientFrameViewTest {
 public:
  BrowserNonClientFrameViewTabbedTest()
      : BrowserNonClientFrameViewTest(Browser::TYPE_NORMAL) {}
};

// TODO(crbug.com/1011339): Flaky on Linux TSAN.
#if (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH) || \
     BUILDFLAG(IS_CHROMEOS_LACROS)) &&                  \
    defined(THREAD_SANITIZER)
#define MAYBE_HitTestTabstrip DISABLED_HitTestTabstrip
#else
#define MAYBE_HitTestTabstrip HitTestTabstrip
#endif

TEST_F(BrowserNonClientFrameViewTabbedTest, MAYBE_HitTestTabstrip) {
  // Add a tab because the browser starts out without any tabs at all.
  AddTab(browser(), GURL("about:blank"));

  const gfx::Rect frame_bounds = frame_view_->bounds();

  gfx::RectF tabstrip_bounds_in_frame_coords(
      frame_view_->browser_view()->tabstrip()->GetLocalBounds());
  views::View::ConvertRectToTarget(frame_view_->browser_view()->tabstrip(),
                                   frame_view_,
                                   &tabstrip_bounds_in_frame_coords);
  const gfx::Rect tabstrip_bounds =
      gfx::ToEnclosingRect(tabstrip_bounds_in_frame_coords);
  EXPECT_FALSE(tabstrip_bounds.IsEmpty());

  // Completely outside the frame's bounds.
  EXPECT_FALSE(frame_view_->HitTestRect(
      gfx::Rect(frame_bounds.x() - 1, frame_bounds.y() + 1, 1, 1)));
  EXPECT_FALSE(frame_view_->HitTestRect(
      gfx::Rect(frame_bounds.x() + 1, frame_bounds.y() - 1, 1, 1)));

  // Hits client portions of the tabstrip (near the bottom left corner of the
  // first tab).
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(
      tabstrip_bounds.x() + 10, tabstrip_bounds.bottom() - 10, 1, 1)));

// Tabs extend to the top of the tabstrip everywhere in this test context on
// ChromeOS, so there is no non-client area in the tab strip to test for.
// TODO (tbergquist): Investigate whether we can key off this condition in an
// OS-agnostic way.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Hits non-client portions of the tab strip (the top left corner of the
  // first tab).
  EXPECT_TRUE(frame_view_->HitTestRect(
      gfx::Rect(tabstrip_bounds.x(), tabstrip_bounds.y(), 1, 1)));
#endif

  // Hits tab strip and the browser-client area.
  EXPECT_TRUE(frame_view_->HitTestRect(
      gfx::Rect(tabstrip_bounds.x() + 1,
                tabstrip_bounds.bottom() -
                    GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) - 1,
                100, 100)));
}
