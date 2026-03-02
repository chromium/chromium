// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/function_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces() {
  std::vector<BrowserWindowInterface*> results;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&results](BrowserWindowInterface* browser) {
        results.push_back(browser);
        return true;
      });
  return results;
}

void ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser) {
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&on_browser](BrowserWindowInterface* browser) {
        return on_browser(browser);
      },
      BrowserCollection::Order::kActivation);
}

void ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser) {
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&on_browser](BrowserWindowInterface* browser) {
        return on_browser(browser);
      },
      BrowserCollection::Order::kActivation, /*enumerate_new_browsers=*/true);
}

BrowserWindowInterface* GetLastActiveBrowserWindowInterfaceWithAnyProfile() {
  BrowserWindowInterface* last_active = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        last_active = browser;
        return false;  // stop iterating
      });
  return last_active;
}
