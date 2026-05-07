// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_FAKE_GLOBAL_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_FAKE_GLOBAL_BROWSER_COLLECTION_H_

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

class FakeGlobalBrowserCollection : public GlobalBrowserCollection {
 public:
  FakeGlobalBrowserCollection() = default;
  ~FakeGlobalBrowserCollection() override = default;

  // Public methods to simulate events.
  void SimulateBrowserCreated(BrowserWindowInterface* browser) {
    GlobalBrowserCollection::OnBrowserCreated(browser);
  }

  void SimulateBrowserClosed(BrowserWindowInterface* browser) {
    GlobalBrowserCollection::OnBrowserClosed(browser);
  }
  void SimulateBrowserActivated(BrowserWindowInterface* browser) {
    GlobalBrowserCollection::OnBrowserActivated(browser);
  }
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_FAKE_GLOBAL_BROWSER_COLLECTION_H_
