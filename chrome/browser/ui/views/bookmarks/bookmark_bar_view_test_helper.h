// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_TEST_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_TEST_HELPER_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

// Used to access private state of BookmarkBarView for testing.
class BookmarkBarViewTestHelper {
 public:
  explicit BookmarkBarViewTestHelper(BookmarkBarView* bbv) : bbv_(bbv) {}
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

 private:
  BookmarkBarView* bbv_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewTestHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_TEST_HELPER_H_
