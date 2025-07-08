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
