// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_OBSERVER_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_OBSERVER_H_

#include "base/observer_list_types.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Browser;

enum class BrowserClosingStatus;

class BrowserListObserver : public base::CheckedObserver {
 public:
  // Called immediately after a browser is added to the list.
  virtual void OnBrowserAdded(Browser* browser) {}

  // Called when code attempted to close `browser`, but failed due to `reason`.
  virtual void OnBrowserCloseCancelled(Browser* browser,
                                       BrowserClosingStatus reason) {}

  // Called when a Browser starts closing. This is called prior to
  // removing the tabs. Removing the tabs may delay or stop the close.
  virtual void OnBrowserClosing(Browser* browser) {}

  // Called immediately after a browser is removed from the list.
  virtual void OnBrowserRemoved(Browser* browser) {}

  // Called immediately after a browser is set active (SetLastActive).
  virtual void OnBrowserSetLastActive(Browser* browser) {}

  // Called immediately after a browser becomes not active.
  virtual void OnBrowserNoLongerActive(Browser* browser) {}
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_OBSERVER_H_
