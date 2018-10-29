// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_utils.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace {

class StarViewTest : public InProcessBrowserTest {
 public:
  StarViewTest() = default;
  ~StarViewTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(StarViewTest);
};

// Verify that clicking the bookmark star a second time hides the bookmark
// bubble.
#if defined(OS_LINUX) && defined(USE_AURA) && !defined(OS_CHROMEOS)
#define MAYBE_HideOnSecondClick DISABLED_HideOnSecondClick
#elif defined(OS_MACOSX)
// Focusing or input is not completely working on Mac: http://crbug.com/824418
#define MAYBE_HideOnSecondClick DISABLED_HideOnSecondClick
#else
#define MAYBE_HideOnSecondClick HideOnSecondClick
#endif
IN_PROC_BROWSER_TEST_F(StarViewTest, MAYBE_HideOnSecondClick) {
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  views::View* star_view = browser_view->toolbar()->location_bar()->star_view();

  ui::MouseEvent pressed_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  // Verify that clicking once shows the bookmark bubble.
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_view->OnMousePressed(pressed_event);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_view->OnMouseReleased(released_event);
  EXPECT_TRUE(BookmarkBubbleView::bookmark_bubble());

  // Verify that clicking again doesn't reshow it.
  star_view->OnMousePressed(pressed_event);
  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  BookmarkBubbleView::Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_view->OnMouseReleased(released_event);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
}

IN_PROC_BROWSER_TEST_F(StarViewTest, InkDropHighlighted) {
  BrowserView* browser_view =
      reinterpret_cast<BrowserView*>(browser()->window());
  StarView* star_view = browser_view->toolbar()->location_bar()->star_view();
  views::test::InkDropHostViewTestApi ink_drop_test_api(star_view);

  if (ink_drop_test_api.HasInkDrop()) {
    GURL url("http://test.com");
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::AddIfNotBookmarked(model, url, /*title=*/base::string16());
    browser_view->ShowBookmarkBubble(url, false);
    EXPECT_EQ(ink_drop_test_api.GetInkDrop()->GetTargetInkDropState(),
              views::InkDropState::ACTIVATED);
  }
}

}  // namespace
