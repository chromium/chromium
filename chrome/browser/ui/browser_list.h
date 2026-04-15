// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_H_

#include <stddef.h>

#include <vector>

#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Browser;

// Maintains a list of Browser objects.
class BrowserList {
 public:
  using BrowserVector = std::vector<raw_ptr<Browser, VectorExperimental>>;
  using const_iterator = BrowserVector::const_iterator;
  using const_reverse_iterator = BrowserVector::const_reverse_iterator;

  BrowserList(const BrowserList&) = delete;
  BrowserList& operator=(const BrowserList&) = delete;

  static BrowserList* GetInstance();

  // Adds or removes |browser| from the list it is associated with. The browser
  // object should be valid BEFORE these calls (for the benefit of observers),
  // so notify and THEN delete the object.
  static void AddBrowser(Browser* browser);
  static void RemoveBrowser(Browser* browser);

  // Appends active browser windows to |browsers_ordered_by_activation_|.
  // Prepends inactive browser windows to |browsers_ordered_by_activation_|.
  static void AddBrowserToActiveList(Browser* browser);

 private:
  friend class Browser;
  friend class BrowserObserverChild;
  FRIEND_TEST_ALL_PREFIXES(BrowserListBrowserTest, ObserverAddedInFlight);

  BrowserList();
  ~BrowserList();

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was on each desktop.
  static void SetLastActive(Browser* browser);

  // Helper method to remove a browser instance from a list of browsers
  static void RemoveBrowserFrom(Browser* browser, BrowserVector* browser_list);

  // A vector of the browsers in this list, in the order they were added.
  BrowserVector browsers_;
  // A vector of the browsers in this list, in reverse order of activation. I.e.
  // the most recently used browser will be at the end. Inactive browser
  // windows, (e.g., created by session restore) are inserted at the front of
  // the list.
  BrowserVector browsers_ordered_by_activation_;

  static BrowserList* instance_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
