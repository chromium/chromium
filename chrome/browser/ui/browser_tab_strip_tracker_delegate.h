// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_STRIP_TRACKER_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_STRIP_TRACKER_DELEGATE_H_

class Browser;

class BrowserTabStripTrackerDelegate {
 public:
  // Returns true if a TabStripModelObserver should be registered for |browser|.
  virtual bool ShouldTrackBrowser(Browser* browser) = 0;

 protected:
  virtual ~BrowserTabStripTrackerDelegate() {}
};

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_STRIP_TRACKER_DELEGATE_H_
