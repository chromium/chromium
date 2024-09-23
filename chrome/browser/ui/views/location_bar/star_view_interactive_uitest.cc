// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/test/button_test_api.h"

namespace {

class StarViewTest : public InProcessBrowserTest {
 public:
  StarViewTest() = default;

  StarViewTest(const StarViewTest&) = delete;
  StarViewTest& operator=(const StarViewTest&) = delete;

  ~StarViewTest() override = default;

  PageActionIconView* GetStarIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kBookmarkStar);
  }
};

// Verifies clicking the star bookmarks the page.
IN_PROC_BROWSER_TEST_F(StarViewTest, BookmarksUrlOnPress) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  PageActionIconView* star_icon = GetStarIcon();
  const GURL current_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();

  // The page should not initiall be bookmarked.
  EXPECT_FALSE(bookmark_model->IsBookmarked(current_url));
  EXPECT_FALSE(star_icon->GetActive());

  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::Point(),
                                gfx::Point(), ui::EventTimeForNow(),
                                ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON);

  static_cast<views::View*>(star_icon)->OnMousePressed(pressed_event);
  static_cast<views::View*>(star_icon)->OnMouseReleased(released_event);

  EXPECT_TRUE(bookmark_model->IsBookmarked(current_url));
  EXPECT_TRUE(star_icon->GetActive());
}

// Verify that clicking the bookmark star a second time hides the bookmark
// bubble.
IN_PROC_BROWSER_TEST_F(StarViewTest, HideOnSecondClick) {
  views::View* star_icon = GetStarIcon();

  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::Point(),
                                gfx::Point(), ui::EventTimeForNow(),
                                ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON);

  // Verify that clicking once shows the bookmark bubble.
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_icon->OnMousePressed(pressed_event);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_icon->OnMouseReleased(released_event);
  EXPECT_TRUE(BookmarkBubbleView::bookmark_bubble());

  // Verify that clicking again doesn't reshow it.
  star_icon->OnMousePressed(pressed_event);
  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  BookmarkBubbleView::Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_icon->OnMouseReleased(released_event);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
}

IN_PROC_BROWSER_TEST_F(StarViewTest, InkDropHighlighted) {
  PageActionIconView* star_icon = GetStarIcon();
  views::test::InkDropHostTestApi ink_drop_test_api(
      views::InkDrop::Get(star_icon));

  if (ink_drop_test_api.HasInkDrop()) {
    GURL url("http://test.com");
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::AddIfNotBookmarked(model, url, /*title=*/std::u16string());
    browser()->window()->ShowBookmarkBubble(url, false);
    EXPECT_EQ(ink_drop_test_api.GetInkDrop()->GetTargetInkDropState(),
              views::InkDropState::ACTIVATED);
  }
}

}  // namespace
