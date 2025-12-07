// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_OBSERVER_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_OBSERVER_H_

#include "base/observer_list_types.h"

class BrowserWindowInterface;

class BrowserCollectionObserver : public base::CheckedObserver {
 public:
  ~BrowserCollectionObserver() override = default;

  virtual void OnBrowserCreated(BrowserWindowInterface* browser) {}
  virtual void OnBrowserClosed(BrowserWindowInterface* browser) {}
  virtual void OnBrowserActivated(BrowserWindowInterface* browser) {}
  virtual void OnBrowserDeactivated(BrowserWindowInterface* browser) {}
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_OBSERVER_H_
