// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_HISTORY_BOOKMARK_MODEL_H_
#define COMPONENTS_BOOKMARKS_BROWSER_HISTORY_BOOKMARK_MODEL_H_

#include <vector>

#include "base/memory/ref_counted.h"

class GURL;

namespace bookmarks {

struct UrlAndTitle;

// Defines the interface use by history. History accesses these functions on a
// background thread.
class HistoryBookmarkModel
    : public base::RefCountedThreadSafe<HistoryBookmarkModel> {
 public:
  HistoryBookmarkModel() {}

  virtual bool IsBookmarked(const GURL& url) = 0;

  // Returns, by reference in |bookmarks|, the set of bookmarked urls and their
  // titles. This returns the unique set of URLs. For example, if two bookmarks
  // reference the same URL only one entry is added not matter the titles are
  // same or not.
  virtual void GetBookmarks(std::vector<UrlAndTitle>* urls) = 0;

 protected:
  friend class base::RefCountedThreadSafe<HistoryBookmarkModel>;

  virtual ~HistoryBookmarkModel() {}
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_HISTORY_BOOKMARK_MODEL_H_
