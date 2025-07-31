// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces() {
  std::vector<BrowserWindowInterface*> results;
  for (BrowserWindowInterface* browser : *BrowserList::GetInstance()) {
    results.push_back(browser);
  }
  return results;
}

std::vector<BrowserWindowInterface*>
GetBrowserWindowInterfacesOrderedByActivation() {
  return std::vector<BrowserWindowInterface*>(
      BrowserList::GetInstance()->begin_browsers_ordered_by_activation(),
      BrowserList::GetInstance()->end_browsers_ordered_by_activation());
}

BrowserWindowInterface* GetLastActiveBrowserWindowInterfaceWithAnyProfile() {
  // TODO(crbug.com/431671448): This is implemented in terms of BrowserList to
  // ensure it stays in sync with other BrowserList APIs during migration. It
  // can be implemented directly once clients are migrated off of BrowserList.
  return BrowserList::GetInstance()->GetLastActive();
}
