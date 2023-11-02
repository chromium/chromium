// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_OBSERVER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_OBSERVER_H_

namespace content {
class WebContents;
}

// Objects implement this interface to get notified about changes in the
// BookmarkTabHelper and to provide necessary functionality.
class BookmarkTabHelperObserver {
 public:
  // Notification that the starredness of the current URL changed.
  virtual void URLStarredChanged(content::WebContents* web_contents,
                                 bool starred) = 0;

 protected:
  virtual ~BookmarkTabHelperObserver() = default;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_OBSERVER_H_
