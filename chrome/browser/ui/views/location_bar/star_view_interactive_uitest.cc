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
#include "chrome/browser/ui/read_later/read_later_test_utils.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/star_menu_model.h"
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
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_utils.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/test/button_test_api.h"

namespace {

class StarViewTest : public InProcessBrowserTest {
 public:
  StarViewTest() = default;
  ~StarViewTest() override = default;

  PageActionIconView* GetStarIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kBookmarkStar);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StarViewTest);
};

// Verifies clicking the star bookmarks the page.
IN_PROC_BROWSER_TEST_F(StarViewTest, BookmarksUrlOnPress) {
  // The url is not bookmarked when the star is pressed when read later is
  // enabled. This test is replaced by
  // StarViewTestWithReadLaterEnabled.AddBookmarkFromStarViewMenuBookmarksUrl.
  if (base::FeatureList::IsEnabled(reading_list::switches::kReadLater))
    return;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  PageActionIconView* star_icon = GetStarIcon();
  const GURL current_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();

  // The page should not initiall be bookmarked.
  EXPECT_FALSE(bookmark_model->IsBookmarked(current_url));
  EXPECT_FALSE(star_icon->GetActive());

  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  static_cast<views::View*>(star_icon)->OnMousePressed(pressed_event);
  static_cast<views::View*>(star_icon)->OnMouseReleased(released_event);

  EXPECT_TRUE(bookmark_model->IsBookmarked(current_url));
  EXPECT_TRUE(star_icon->GetActive());
}

// Verify that clicking the bookmark star a second time hides the bookmark
// bubble.
IN_PROC_BROWSER_TEST_F(StarViewTest, HideOnSecondClick) {
  // The BookmarkBubbleView is not shown when the StarView is first pressed when
  // the reading list is enabled.
  if (base::FeatureList::IsEnabled(reading_list::switches::kReadLater))
    return;

  views::View* star_icon = GetStarIcon();

  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

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
  views::test::InkDropHostViewTestApi ink_drop_test_api(star_icon);

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

class StarViewTestWithReadLaterEnabled : public InProcessBrowserTest {
 public:
  StarViewTestWithReadLaterEnabled() {
    feature_list_.InitAndEnableFeature(reading_list::switches::kReadLater);
  }
  StarViewTestWithReadLaterEnabled(const StarViewTestWithReadLaterEnabled&) =
      delete;
  StarViewTestWithReadLaterEnabled& operator=(
      const StarViewTestWithReadLaterEnabled&) = delete;
  ~StarViewTestWithReadLaterEnabled() override = default;

  StarView* GetStarIcon() {
    return static_cast<StarView*>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kBookmarkStar));
  }

  void OpenStarViewMenu(StarView* star_icon) {
    ui::MouseEvent pressed_event(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON);

    views::test::ButtonTestApi(star_icon).NotifyClick(pressed_event);
    views::test::ButtonTestApi(star_icon).NotifyClick(released_event);
    views::MenuRunner* menu_runner = star_icon->menu_runner_for_test();
    EXPECT_TRUE(menu_runner->IsRunning());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies clicking the bookmark button in the StarView's menu bookmarks the
// page.
IN_PROC_BROWSER_TEST_F(StarViewTestWithReadLaterEnabled,
                       AddBookmarkFromStarViewMenuBookmarksUrl) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  StarView* star_icon = GetStarIcon();
  const GURL current_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();

  // The page should not initially be bookmarked.
  EXPECT_FALSE(bookmark_model->IsBookmarked(current_url));
  EXPECT_FALSE(star_icon->GetActive());

  OpenStarViewMenu(star_icon);

  // The page should not be bookmarked when the menu is opened.
  EXPECT_FALSE(bookmark_model->IsBookmarked(current_url));
  EXPECT_FALSE(star_icon->GetActive());

  StarMenuModel* menu_model = star_icon->menu_model_for_test();

  // Activate "Add Bookmark" button in the menu and verify the bookmark is
  // added.
  const int bookmark_command_index =
      menu_model->GetIndexOfCommandId(StarMenuModel::CommandBookmark);
  menu_model->ActivatedAt(bookmark_command_index);

  EXPECT_TRUE(bookmark_model->IsBookmarked(current_url));
  EXPECT_TRUE(star_icon->GetActive());
}

// Verifies clicking the Read Later button in the StarView's menu saves the page
// to the read later model.
IN_PROC_BROWSER_TEST_F(StarViewTestWithReadLaterEnabled,
                       AddToReadLaterFromStarViewMenuSavesUrlToReadLater) {
  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(browser()->profile());
  test::ReadingListLoadObserver(reading_list_model).Wait();
  GURL url("http://www.test.com/");
  ui_test_utils::NavigateToURL(browser(), url);

  StarView* star_icon = GetStarIcon();
  const GURL current_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();

  // The page should not initially be in model.
  EXPECT_EQ(reading_list_model->GetEntryByURL(current_url), nullptr);
  EXPECT_FALSE(star_icon->GetActive());

  OpenStarViewMenu(star_icon);

  // The page should not be bookmarked when the menu is opened.
  EXPECT_EQ(reading_list_model->GetEntryByURL(current_url), nullptr);
  EXPECT_FALSE(star_icon->GetActive());

  StarMenuModel* menu_model = star_icon->menu_model_for_test();

  // // Activate "Add to Read later" button in the menu and verify the entry is
  // added.
  const int read_later_command_index =
      menu_model->GetIndexOfCommandId(StarMenuModel::CommandMoveToReadLater);
  menu_model->ActivatedAt(read_later_command_index);

  EXPECT_NE(reading_list_model->GetEntryByURL(current_url), nullptr);
  EXPECT_FALSE(star_icon->GetActive());
}

}  // namespace
