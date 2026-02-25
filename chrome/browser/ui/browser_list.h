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
#include "base/observer_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Browser;
class BrowserWindowInterface;
class BrowserListObserver;

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

  // Adds and removes |observer| from the observer list for all desktops.
  // Observers are responsible for making sure the notifying browser is relevant
  // to them (e.g., on the specific desktop they care about if any).
  static void AddObserver(BrowserListObserver* observer);
  static void RemoveObserver(BrowserListObserver* observer);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was on each desktop.
  static void SetLastActive(Browser* browser);

  // Notifies the observers when the current active browser becomes not active.
  static void NotifyBrowserNoLongerActive(Browser* browser);

 private:
  friend class BrowserObserverChild;
  FRIEND_TEST_ALL_PREFIXES(BrowserListBrowserTest, ObserverAddedInFlight);

  BrowserList();
  ~BrowserList();

  const_iterator deprecated_begin() const { return browsers_.begin(); }
  const_iterator deprecated_end() const { return browsers_.end(); }

  // Returns iterated access to list of open browsers ordered by activation. The
  // underlying data structure is a vector and we push_back on recent access so
  // a reverse iterator gives the latest accessed browser first.
  //
  // These functions are deprecated and should only be used by
  // browser_window_interface_iterator_non_android.cc's
  // ForEachCurrentBrowserWindowInterfaceOrderedByActivation() and
  // ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation()
  const_reverse_iterator deprecated_begin_browsers_ordered_by_activation()
      const {
    return browsers_ordered_by_activation_.rbegin();
  }
  const_reverse_iterator deprecated_end_browsers_ordered_by_activation() const {
    return browsers_ordered_by_activation_.rend();
  }

  // Helper method to remove a browser instance from a list of browsers
  static void RemoveBrowserFrom(Browser* browser, BrowserVector* browser_list);

  // A vector of the browsers in this list, in the order they were added.
  BrowserVector browsers_;
  // A vector of the browsers in this list, in reverse order of activation. I.e.
  // the most recently used browser will be at the end. Inactive browser
  // windows, (e.g., created by session restore) are inserted at the front of
  // the list.
  BrowserVector browsers_ordered_by_activation_;

  // If an observer is added while iterating over them and notifying, it should
  // not be notified as it probably already saw the Browser* being added/removed
  // in the BrowserList.
  struct ObserverListTraits : base::internal::LeakyLazyInstanceTraits<
                                  base::ObserverList<BrowserListObserver>> {
    static base::ObserverList<BrowserListObserver>* New(void* instance) {
      return new (instance) base::ObserverList<BrowserListObserver>(
          base::ObserverListPolicy::EXISTING_ONLY);
    }
  };

  // A list of observers which will be notified of every browser addition and
  // removal across all BrowserLists.
  static base::LazyInstance<base::ObserverList<BrowserListObserver>,
                            ObserverListTraits>
      observers_;

  static BrowserList* instance_;

  // These browser_window_interface_iterator_non_android.cc functions need
  // access to deprecated_begin_browsers_ordered_by_activation() and
  // deprecated_end_browsers_ordered_by_activation().
  friend void ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      base::FunctionRef<bool(BrowserWindowInterface*)> on_browser);
  friend void ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      base::FunctionRef<bool(BrowserWindowInterface*)> on_browser);
  // BrowserListEnumerator needs access to begin() and end().
  friend class BrowserListEnumerator;
  // GetAllBrowserWindowInterfaces() needs access to begin() and end().
  friend std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces();
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
