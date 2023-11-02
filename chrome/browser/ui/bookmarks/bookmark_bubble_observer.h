// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_OBSERVER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_OBSERVER_H_

namespace bookmarks {

class BookmarkNode;

// Observes the lifetime of a bookmark bubble.
class BookmarkBubbleObserver {
 public:
  virtual ~BookmarkBubbleObserver() {}

  virtual void OnBookmarkBubbleShown(const BookmarkNode* node) = 0;
  virtual void OnBookmarkBubbleHidden() = 0;
};

}  // namespace bookmarks

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_OBSERVER_H_
