// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_ENUMERATOR_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_ENUMERATOR_H_

#include "build/build_config.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Browser;

// Enumerates each browser in the system, accounting for browsers that are
// removed or added while enumerating. Added browsers will be included if
// |enumerate_new_browser| is true. Note that the newly-added browsers will be
// added at the end of the list, regardless of the original ordering.
//
// Use to iterate through browsers and perform operations that might add or
// remove browsers.
class BrowserListEnumerator : public BrowserListObserver {
 public:
  explicit BrowserListEnumerator(bool enumerate_new_browser = false);
  BrowserListEnumerator(BrowserList::BrowserVector browser_list,
                        bool enumerate_new_browser);

  BrowserListEnumerator(const BrowserListEnumerator&) = delete;
  BrowserListEnumerator& operator=(const BrowserListEnumerator&) = delete;
  ~BrowserListEnumerator() override;

  bool empty() const { return browsers_.empty(); }

  Browser* Next();

 private:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  bool enumerate_new_browser_;
  BrowserList::BrowserVector browsers_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_ENUMERATOR_H_
