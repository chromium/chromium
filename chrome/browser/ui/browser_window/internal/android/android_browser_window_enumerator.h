// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_ENUMERATOR_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_ENUMERATOR_H_

#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class BrowserWindowInterface;
class GlobalBrowserCollection;

// Enumerates each android browser window in the system in activation order in a
// way that is resilient to additions and removals during iteration. Note that
// any windows added during iteration (if |enumerate_new_browser_windows| is
// true) will be added to the end of the list, even if that is not the correct
// activation order.
//
// This is roughly the Android equivalent of
// chrome/browser/ui/browser_list_enumerator.h and the interface and
// implementation is heavily based on BrowserListEnumerator.
//
// This class is only safe to use as a thread-local variable.
class AndroidBrowserWindowEnumerator : public BrowserCollectionObserver {
 public:
  AndroidBrowserWindowEnumerator(
      std::vector<BrowserWindowInterface*> browser_windows,
      bool enumerate_new_browser_windows = false);
  AndroidBrowserWindowEnumerator(const AndroidBrowserWindowEnumerator&) =
      delete;
  AndroidBrowserWindowEnumerator& operator=(
      const AndroidBrowserWindowEnumerator&) = delete;
  ~AndroidBrowserWindowEnumerator() override;

  bool empty() const { return browser_windows_.empty(); }

  BrowserWindowInterface* Next();

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  std::vector<BrowserWindowInterface*> browser_windows_;
  bool enumerate_new_browser_windows_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_ENUMERATOR_H_
