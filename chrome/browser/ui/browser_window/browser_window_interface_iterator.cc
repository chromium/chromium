// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces() {
  std::vector<BrowserWindowInterface*> results;
  for (BrowserWindowInterface* browser : *BrowserList::GetInstance()) {
    results.push_back(browser);
  }
  return results;
}
