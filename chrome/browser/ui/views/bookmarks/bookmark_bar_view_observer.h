// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_OBSERVER_H_

class BookmarkBarViewObserver {
 public:
  virtual void OnBookmarkBarVisibilityChanged() = 0;

 protected:
  ~BookmarkBarViewObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_OBSERVER_H_
