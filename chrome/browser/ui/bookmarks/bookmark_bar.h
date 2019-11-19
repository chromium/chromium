// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_H_

#include "base/macros.h"

class BookmarkBar {
 public:
  enum State {
    // The bookmark bar is not visible.
    HIDDEN,

    // The bookmark bar is visible.
    SHOW
  };

  // Used when the state changes to indicate if the transition should be
  // animated.
  enum AnimateChangeType {
    ANIMATE_STATE_CHANGE,
    DONT_ANIMATE_STATE_CHANGE
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BookmarkBar);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_H_
