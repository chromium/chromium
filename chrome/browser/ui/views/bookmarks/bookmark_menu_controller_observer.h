// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_OBSERVER_H_

class BookmarkMenuController;

// The observer is notified prior to the menu being deleted.
class BookmarkMenuControllerObserver {
 public:
  virtual void BookmarkMenuControllerDeleted(
      BookmarkMenuController* controller) = 0;

 protected:
  virtual ~BookmarkMenuControllerObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_CONTROLLER_OBSERVER_H_
