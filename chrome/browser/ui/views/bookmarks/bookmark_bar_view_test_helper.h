// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_TEST_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"

// Used to access private state of BookmarkBarView for testing.
class BookmarkBarViewTestHelper {
 public:
  explicit BookmarkBarViewTestHelper(BookmarkBarView* bbv) : bbv_(bbv) {}

  BookmarkBarViewTestHelper(const BookmarkBarViewTestHelper&) = delete;
  BookmarkBarViewTestHelper& operator=(const BookmarkBarViewTestHelper&) =
      delete;

  ~BookmarkBarViewTestHelper() {}

  size_t GetBookmarkButtonCount() { return bbv_->bookmark_buttons_.size(); }

  views::LabelButton* GetBookmarkButton(size_t index) {
    return bbv_->bookmark_buttons_[index];
  }

  views::LabelButton* apps_page_shortcut() { return bbv_->apps_page_shortcut_; }

  views::MenuButton* overflow_button() { return bbv_->overflow_button_; }

  views::MenuButton* managed_bookmarks_button() {
    return bbv_->managed_bookmarks_button_;
  }

  tab_groups::SavedTabGroupBar* saved_tab_group_bar() {
    return bbv_->saved_tab_group_bar_;
  }

  const views::View* saved_tab_groups_separator_view_() const {
    return bbv_->GetSavedTabGroupsSeparatorViewForTesting();
  }

  int GetDropLocationModelIndexForTesting() {
    return bbv_->GetDropLocationModelIndexForTesting();
  }

 private:
  raw_ptr<BookmarkBarView, AcrossTasksDanglingUntriaged> bbv_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_TEST_HELPER_H_
