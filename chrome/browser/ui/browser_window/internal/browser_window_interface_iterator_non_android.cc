// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/function_ref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_enumerator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces() {
  std::vector<BrowserWindowInterface*> results;
  for (BrowserWindowInterface* browser : *BrowserList::GetInstance()) {
    results.push_back(browser);
  }
  return results;
}

void ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser) {
  // Make a copy of the BrowserList to simplify the case where we need to
  // add or remove a Browser during the loop.
  constexpr bool kEnumerateNewBrowser = false;
  BrowserListEnumerator browser_list_copy(
      BrowserList::BrowserVector(
          BrowserList::GetInstance()->begin_browsers_ordered_by_activation(),
          BrowserList::GetInstance()->end_browsers_ordered_by_activation()),
      kEnumerateNewBrowser);
  while (!browser_list_copy.empty()) {
    Browser* browser = browser_list_copy.Next();

    // Skip browsers that are already scheduled for deletion to prevent
    // dangling pointer issues during browser destruction.
    if (browser->is_delete_scheduled()) {
      continue;  // Skip this browser and continue with the next one
    }

    if (!on_browser(browser)) {
      break;
    }
  }
}

void ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser) {
  // Make a copy of the BrowserList to simplify the case where we need to
  // add or remove a Browser during the loop.
  constexpr bool kEnumerateNewBrowser = true;
  BrowserListEnumerator browser_list_copy(
      BrowserList::BrowserVector(
          BrowserList::GetInstance()->begin_browsers_ordered_by_activation(),
          BrowserList::GetInstance()->end_browsers_ordered_by_activation()),
      kEnumerateNewBrowser);
  while (!browser_list_copy.empty()) {
    Browser* browser = browser_list_copy.Next();

    // Skip browsers that are already scheduled for deletion to prevent
    // dangling pointer issues during browser destruction.
    if (browser->is_delete_scheduled()) {
      continue;  // Skip this browser and continue with the next one
    }

    if (!on_browser(browser)) {
      break;
    }
  }
}

BrowserWindowInterface* GetLastActiveBrowserWindowInterfaceWithAnyProfile() {
  // TODO(crbug.com/431671448): This is implemented in terms of BrowserList to
  // ensure it stays in sync with other BrowserList APIs during migration. It
  // can be implemented directly once clients are migrated off of BrowserList.
  return BrowserList::GetInstance()->GetLastActive();
}
