// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

namespace chrome {

BrowserTabMenuModelDelegate::BrowserTabMenuModelDelegate(Browser* browser)
    : browser_(browser) {}

BrowserTabMenuModelDelegate::~BrowserTabMenuModelDelegate() = default;

std::vector<Browser*>
BrowserTabMenuModelDelegate::GetExistingWindowsForMoveMenu() {
  std::vector<Browser*> browsers;

  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    // We can only move into a tabbed view of the same profile, and not the same
    // window we're currently in.
    if (browser != browser_ && browser->is_type_normal() &&
        browser->profile() == browser_->profile()) {
      browsers.push_back(browser);
    }
  }
  return browsers;
}

}  // namespace chrome
