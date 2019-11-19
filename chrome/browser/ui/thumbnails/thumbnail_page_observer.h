// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_PAGE_OBSERVER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_PAGE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "url/gurl.h"

namespace thumbnails {

// Observer of navigation and loading events relevant to thumbnail processing.
//
// Note that while most page loads receive the following signals in order:
//   - Navigation Started
//   - Navigation Ended
//   - Page Load Started
//   - Page Load Finished
//
// Not all page transitions will receive all of these or in that order.
// Classes implementing this interface should be able to handle a navigation end
// without a start, or a frame load without a prior navigation.
class ThumbnailPageObserver : public base::CheckedObserver {
 public:
  // Called when navigation in the top-level browser window starts.
  virtual void TopLevelNavigationStarted(const GURL& url) = 0;

  // Called when navigation in the top-level browser window completes.
  virtual void TopLevelNavigationEnded(const GURL& url) = 0;

  // Called when the page/tab's visibility changes.
  virtual void VisibilityChanged(bool visible) = 0;

  // Called when the page is painted for the first time.
  virtual void PagePainted() = 0;

  // Called when a page begins to load.
  virtual void PageLoadStarted() = 0;

  // Called when a page finishes loading.
  virtual void PageLoadFinished() = 0;
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_PAGE_OBSERVER_H_
