// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_H_

class BookmarkBar {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum State {
    // The bookmark bar is not visible.
    HIDDEN = 0,

    // The bookmark bar is visible.
    SHOW = 1,

    // Constant used by the histogram macros.
    kMaxValue = SHOW
  };

  // Used when the state changes to indicate if the transition should be
  // animated.
  enum AnimateChangeType {
    ANIMATE_STATE_CHANGE,
    DONT_ANIMATE_STATE_CHANGE
  };

  BookmarkBar() = delete;
  BookmarkBar(const BookmarkBar&) = delete;
  BookmarkBar& operator=(const BookmarkBar&) = delete;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_H_
